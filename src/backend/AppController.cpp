#include "AppController.h"

#include "Formatting.h"

#include "savevault/core/Uuid.h"
#include "savevault/core/Version.h"
#include "savevault/detection/Launchers.h"
#include "savevault/registry/RegistryCache.h"
#include "savevault/registry/RegistryUpdater.h"
#include "savevault/registry/Fetcher.h"
#include "savevault/services/BackupService.h"
#include "savevault/services/GameArt.h"
#include "savevault/services/LocalProfiles.h"
#include "savevault/platform/Processes.h"
#include "savevault/services/SaveSlots.h"
#include "savevault/services/Transfer.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>
#include <QFileInfo>
#include <QLocale>
#include <QJsonDocument>
#include <QTime>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace savevault::backend {

namespace {

constexpr qsizetype kMaxActivity = 200;

// What a scan hands back to the interface thread. The catalog itself (every
// registry profile) stays on the worker and is freed there: only the games
// found carry their profile.
struct RefreshResult {
    int profileCount = 0;
    bool usingTestKeys = false;
    QStringList warnings;
    QList<registry::Notice> notices;
    services::LibraryScan scan;
    QHash<QString, services::GameArt> art;
};

// Art found on this machine plus covers downloaded earlier.
QHash<QString, services::GameArt> findArt(const QList<services::LibraryGame> &games,
                                          const detection::DetectionEnvironment &env, const QString &coverDir)
{
    const services::LocalArtFinder finder(detection::steamRoots(env));
    const services::SteamArtCache cache(coverDir);
    QHash<QString, services::GameArt> art;
    for (const services::LibraryGame &game : games) {
        services::GameArt found = finder.find(game);
        if (found.header.isEmpty()) {
            for (const QString &appId : services::steamAppIds(game)) {
                found.header = cache.cachedHeader(appId);
                if (!found.header.isEmpty())
                    break;
            }
        }
        if (!found.isEmpty())
            art.insert(game.id(), found);
    }
    return art;
}

struct RegistryUpdateResult {
    QList<registry::ChannelUpdate> channels;
    QString error; // set when no update could be attempted at all
};

// Applies the retention policy and frees the space of blobs nothing references;
// with no policy, only frees that space.
Result<services::RetentionReport> cleanUp(vault::Vault &vault, const services::RetentionPolicy &policy)
{
    if (!policy.keepsEverything())
        return services::applyRetention(vault, policy);
    auto garbage = vault.collectGarbage();
    if (!garbage.ok())
        return garbage.error();
    services::RetentionReport report;
    report.freedBytes = garbage.value().freedBytes;
    return report;
}

// Automatic checks run at most this often.
constexpr qint64 kRegistryCheckIntervalSecs = 24 * 60 * 60;

// How often running games and the backup schedule are checked.
constexpr int kAutoBackupCheckMs = 15'000;

struct AutoBackupResult {
    QList<services::LibraryGame> updated;
    QStringList names;     // games backed up, for the activity log
    QStringList failures;
};

struct BackupAllResult {
    QList<services::LibraryGame> updated;
    QStringList failures;
};

services::CloudPolicy toPolicy(const QString &policy)
{
    if (policy == u"prefer-savevault")
        return services::CloudPolicy::PreferSaveVault;
    if (policy == u"prefer-launcher")
        return services::CloudPolicy::PreferLauncher;
    return services::CloudPolicy::Ask;
}

QString actionName(services::FileAction action)
{
    switch (action) {
    case services::FileAction::Create: return QStringLiteral("create");
    case services::FileAction::Overwrite: return QStringLiteral("overwrite");
    case services::FileAction::Unchanged: return QStringLiteral("unchanged");
    case services::FileAction::Skip: return QStringLiteral("skip");
    }
    return {};
}

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    openVault();
    // Scan once the interface is up, so the window appears immediately.
    QTimer::singleShot(0, this, &AppController::refresh);
    m_autoTimer.setInterval(kAutoBackupCheckMs);
    connect(&m_autoTimer, &QTimer::timeout, this, &AppController::checkAutomaticBackups);
    m_autoTimer.start();
}

AppController::~AppController()
{
    if (m_running)
        m_running->waitForFinished();
    if (m_coverDownload)
        m_coverDownload->waitForFinished();
}

QString AppController::appVersion() const
{
    return QString::fromUtf8(kVersion.data(), static_cast<qsizetype>(kVersion.size()));
}

services::ServiceEnvironment AppController::serviceEnvironment()
{
    services::ServiceEnvironment env;
    env.machineId = m_settings.installationId();
    env.appVersion = appVersion();
    return env;
}

void AppController::openVault()
{
    m_vault.reset();
    auto opened = vault::Vault::open(m_settings.vaultPath());
    if (opened.ok()) {
        m_vault.emplace(std::move(opened).value());
        m_vaultError.clear();
    } else {
        m_vaultError = opened.error().message;
    }
    emit vaultChanged();
}

void AppController::log(const QString &text, const QString &kind)
{
    m_activity.prepend(QVariantMap{{QStringLiteral("time"), QTime::currentTime().toString(QStringLiteral("HH:mm"))},
                                   {QStringLiteral("text"), text},
                                   {QStringLiteral("kind"), kind}});
    while (m_activity.size() > kMaxActivity)
        m_activity.removeLast();
    emit activityChanged();
    emit notify(text, kind);
}

template <typename T>
void AppController::run(const QString &busyText, std::function<T()> work, std::function<void(T)> done)
{
    if (busy()) {
        log(tr("Aguarde a operação atual terminar."), QStringLiteral("warning"));
        return;
    }
    m_busyText = busyText;
    emit busyChanged();

    auto *watcher = new QFutureWatcher<T>(this);
    m_running = watcher;
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, done] {
        m_running = nullptr;
        m_busyText.clear();
        emit busyChanged();
        done(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

void AppController::refresh()
{
    const QString cacheDir = m_settings.registryCacheDirectory();
    const QString localDir = m_settings.localProfilesDirectory();
    const QString version = appVersion();
    const vault::Vault *vaultPtr = m_vault ? &*m_vault : nullptr;
    run<RefreshResult>(
        tr("Procurando jogos…"),
        [cacheDir, localDir, coverDir = m_settings.coverCacheDirectory(), version, vaultPtr] {
            RefreshResult result;
            {
                services::CatalogLoad load = services::loadCatalogFromCache(cacheDir, version, localDir);
                const QList<registry::CatalogEntry> entries = load.catalog.entries();
                result.profileCount = static_cast<int>(entries.size());
                result.usingTestKeys = load.usingTestKeys;
                result.warnings = load.warnings;
                result.notices = load.notices;
                const detection::DetectionEnvironment env = detection::DetectionEnvironment::host();
                result.scan = services::scanLibrary(entries, detection::detectInstalledGames(env), env, vaultPtr);
            }
            const detection::DetectionEnvironment env = detection::DetectionEnvironment::host();
            result.art = findArt(result.scan.games, env, coverDir);
            return result;
        },
        [this](RefreshResult result) {
            m_profileCount = result.profileCount;
            m_usingTestKeys = result.usingTestKeys;
            m_warnings = result.warnings;
            m_notices.clear();
            for (const registry::Notice &notice : std::as_const(result.notices))
                m_notices.append(notice.attribution);
            m_notices.removeDuplicates();
            emit catalogChanged();
            m_library.setGames(result.scan.games);
            m_library.setArt(result.art);
            log(countText(result.scan.games.size(), tr("jogo encontrado"), tr("jogos encontrados")), QStringLiteral("info"));
            downloadMissingCovers();
            maybeUpdateRegistry();
        });
}

void AppController::setBackupOnGameExit(bool enabled)
{
    if (enabled == m_settings.backupOnGameExit())
        return;
    m_settings.setBackupOnGameExit(enabled);
    emit autoBackupChanged();
}

void AppController::setBackupIntervalHours(int hours)
{
    if (hours == m_settings.backupIntervalHours())
        return;
    m_settings.setBackupIntervalHours(hours);
    // A new schedule starts counting now.
    m_settings.setLastScheduledBackup(QDateTime::currentDateTimeUtc());
    emit autoBackupChanged();
}

void AppController::setRunInBackground(bool enabled)
{
    if (enabled == m_settings.runInBackground())
        return;
    m_settings.setRunInBackground(enabled);
    emit autoBackupChanged();
}

void AppController::setTrayAvailable(bool available)
{
    if (available == m_trayAvailable)
        return;
    m_trayAvailable = available;
    emit trayAvailableChanged();
}

void AppController::checkAutomaticBackups()
{
    if (!m_vault || m_library.count() == 0)
        return;
    const auto enqueue = [this](const QString &gameId, vault::SnapshotTrigger trigger) {
        const bool queued = std::any_of(m_autoQueue.cbegin(), m_autoQueue.cend(),
                                        [&gameId](const auto &entry) { return entry.first == gameId; });
        if (!queued)
            m_autoQueue.append({gameId, trigger});
    };

    // Always track running games, so turning the option on later works at once.
    const QStringList exited = m_exitTracker.update(m_library.games(), platform::runningProcesses());
    if (m_settings.backupOnGameExit()) {
        for (const QString &gameId : exited)
            enqueue(gameId, vault::SnapshotTrigger::GameExit);
    }

    const int hours = m_settings.backupIntervalHours();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime last = m_settings.lastScheduledBackup();
    if (hours > 0 && (!last.isValid() || last.secsTo(now) >= qint64(hours) * 3600)) {
        m_settings.setLastScheduledBackup(now);
        for (const services::LibraryGame &game : m_library.games()) {
            // A game that is running is backed up when it closes instead.
            if (!m_exitTracker.running().contains(game.id()))
                enqueue(game.id(), vault::SnapshotTrigger::Schedule);
        }
    }
    runAutomaticBackups();
    maybeRunMaintenance();
}

services::RetentionPolicy AppController::retentionPolicy() const
{
    return {m_settings.retentionKeepLast(), m_settings.retentionKeepDays()};
}

void AppController::setRetentionKeepLast(int count)
{
    if (count == m_settings.retentionKeepLast())
        return;
    m_settings.setRetentionKeepLast(count);
    emit retentionChanged();
}

void AppController::setRetentionKeepDays(int days)
{
    if (days == m_settings.retentionKeepDays())
        return;
    m_settings.setRetentionKeepDays(days);
    emit retentionChanged();
}

QString AppController::maintenanceText() const
{
    const QDateTime last = m_settings.lastMaintenance();
    return last.isValid() ? tr("Última verificação e limpeza: %1.").arg(relativeTime(last))
                          : tr("O vault ainda não foi verificado.");
}

void AppController::maybeRunMaintenance()
{
    constexpr qint64 kWeekSecs = 7 * 24 * 3600;
    const QDateTime last = m_settings.lastMaintenance();
    if (!m_vault || busy() || !m_autoQueue.isEmpty()
        || (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < kWeekSecs))
        return;
    verifyVault();
}

void AppController::verifyVault()
{
    if (!m_vault)
        return;
    vault::Vault *vaultPtr = &*m_vault;
    struct Maintenance {
        Result<vault::VerifyReport> verify = Error{ErrorCode::Unknown, {}, {}};
        std::optional<Result<services::RetentionReport>> cleanup;
    };
    run<Maintenance>(
        tr("Verificando o vault…"),
        [vaultPtr, policy = retentionPolicy()] {
            Maintenance result;
            result.verify = vaultPtr->verify();
            // Clean up only a healthy vault: damaged snapshots hide what is referenced.
            if (result.verify.ok() && result.verify.value().healthy())
                result.cleanup = cleanUp(*vaultPtr, policy);
            return result;
        },
        [this](Maintenance result) {
            m_settings.setLastMaintenance(QDateTime::currentDateTimeUtc());
            emit retentionChanged();
            if (!result.verify.ok()) {
                log(tr("Não foi possível verificar o vault: %1").arg(result.verify.error().message), QStringLiteral("error"));
                return;
            }
            const vault::VerifyReport &report = result.verify.value();
            if (!report.healthy()) {
                log(tr("O vault tem problemas: %1 com conteúdo danificado, %2 faltando e %3 ilegíveis. "
                       "Os backups afetados podem não restaurar; faça um novo backup dos jogos.")
                        .arg(countText(report.corruptedBlobs.size(), tr("arquivo"), tr("arquivos")))
                        .arg(report.missingBlobs.size())
                        .arg(report.damagedSnapshotFiles.size()),
                    QStringLiteral("error"));
                return;
            }
            log(tr("Vault verificado: %1 sem problemas.")
                    .arg(countText(report.blobsChecked, tr("arquivo"), tr("arquivos"))),
                QStringLiteral("success"));
        });
}

void AppController::cleanUpVault()
{
    if (!m_vault)
        return;
    vault::Vault *vaultPtr = &*m_vault;
    run<Result<services::RetentionReport>>(
        tr("Limpando backups antigos…"),
        [vaultPtr, policy = retentionPolicy()] { return cleanUp(*vaultPtr, policy); },
        [this](Result<services::RetentionReport> report) {
            if (!report.ok()) {
                log(tr("A limpeza falhou: %1").arg(report.error().message), QStringLiteral("error"));
                return;
            }
            log(tr("%1 removido, %2 liberados.")
                    .arg(countText(report.value().removedSnapshots, tr("backup antigo"), tr("backups antigos")),
                         QLocale(QLocale::Portuguese, QLocale::Brazil).formattedDataSize(report.value().freedBytes)),
                QStringLiteral("success"));
            refresh();
        });
}

void AppController::setSnapshotPinned(const QString &gameId, const QString &snapshotId, bool pinned)
{
    if (!m_vault || busy())
        return;
    auto updated = m_vault->setPinned(gameId, QUuid::fromString(snapshotId), pinned);
    if (!updated.ok()) {
        log(tr("Não foi possível alterar o backup: %1").arg(updated.error().message), QStringLiteral("error"));
        return;
    }
    emit slotsChanged(gameId); // the game page reloads its history
}

void AppController::deleteSnapshot(const QString &gameId, const QString &snapshotId)
{
    if (!m_vault || busy())
        return;
    if (auto removed = m_vault->removeSnapshot(gameId, QUuid::fromString(snapshotId)); !removed.ok()) {
        log(tr("Não foi possível apagar o backup: %1").arg(removed.error().message), QStringLiteral("error"));
        return;
    }
    if (const services::LibraryGame *game = m_library.game(gameId)) {
        services::LibraryGame copy = *game;
        const QList<vault::Snapshot> remaining = m_vault->snapshots(gameId);
        copy.snapshotCount = remaining.size();
        copy.lastSnapshot = remaining.isEmpty() ? std::nullopt : std::optional<vault::Snapshot>(remaining.last());
        m_library.updateGame(copy);
    }
    log(tr("Backup apagado."), QStringLiteral("info"));
    emit slotsChanged(gameId);
}

void AppController::runAutomaticBackups()
{
    if (m_autoQueue.isEmpty() || busy() || !m_vault)
        return;
    struct Job {
        services::LibraryGame game;
        vault::SnapshotTrigger trigger;
        QString slot;
    };
    QList<Job> jobs;
    for (const auto &[gameId, trigger] : std::as_const(m_autoQueue)) {
        if (const services::LibraryGame *game = m_library.game(gameId))
            jobs.append({*game, trigger, m_settings.activeSlot(gameId)});
    }
    m_autoQueue.clear();
    if (jobs.isEmpty())
        return;

    vault::Vault *vaultPtr = &*m_vault;
    const services::ServiceEnvironment env = serviceEnvironment();
    run<AutoBackupResult>(
        tr("Backup automático…"),
        [vaultPtr, env, jobs, policy = retentionPolicy()] {
            AutoBackupResult result;
            services::BackupService service(*vaultPtr, env);
            for (Job job : jobs) {
                const vault::Snapshot *last = job.game.lastSnapshot ? &*job.game.lastSnapshot : nullptr;
                if (!services::savesChangedSince(job.game.target, last))
                    continue;
                services::BackupOptions options;
                options.kind = vault::SnapshotKind::Auto;
                options.trigger = job.trigger;
                options.slot = job.slot;
                auto snapshot = service.backup(job.game.target, options);
                if (snapshot.ok()) {
                    (void)services::applyRetention(*vaultPtr, policy, {job.game.id()}, QDateTime::currentDateTimeUtc(), false);
                    job.game.lastSnapshot = snapshot.value();
                    job.game.snapshotCount = vaultPtr->snapshots(job.game.id()).size();
                    result.updated.append(job.game);
                    result.names.append(job.game.target.profile.name);
                } else if (snapshot.error().code != ErrorCode::NotFound
                           && snapshot.error().code != ErrorCode::PermissionDenied) {
                    // Missing saves and running games are retried at the next trigger.
                    result.failures.append(job.game.target.profile.name + QStringLiteral(": ") + snapshot.error().message);
                }
            }
            return result;
        },
        [this](AutoBackupResult result) {
            for (const services::LibraryGame &game : std::as_const(result.updated))
                m_library.updateGame(game);
            for (const QString &failure : std::as_const(result.failures))
                log(tr("Backup automático falhou: %1").arg(failure), QStringLiteral("error"));
            if (!result.names.isEmpty()) {
                const QString text = result.names.size() == 1
                                         ? tr("Backup automático de %1 salvo.").arg(result.names.first())
                                         : tr("Backup automático salvo: %1.").arg(result.names.join(QStringLiteral(", ")));
                log(text, QStringLiteral("success"));
                emit automaticBackupDone(text);
            }
            // Triggers that arrived meanwhile.
            runAutomaticBackups();
        });
}

void AppController::setThemeMode(const QString &mode)
{
    if (mode != QLatin1String("system") && mode != QLatin1String("dark") && mode != QLatin1String("light"))
        return;
    if (mode == m_settings.themeMode())
        return;
    m_settings.setThemeMode(mode);
    emit appearanceChanged();
}

void AppController::setAccentColor(const QString &color)
{
    static const QRegularExpression hex(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    if (!hex.match(color).hasMatch() || color.compare(m_settings.accentColor(), Qt::CaseInsensitive) == 0)
        return;
    m_settings.setAccentColor(color.toUpper());
    emit appearanceChanged();
}

void AppController::setDownloadCovers(bool enabled)
{
    if (enabled == m_settings.downloadCovers())
        return;
    m_settings.setDownloadCovers(enabled);
    emit coversChanged();
    if (enabled)
        downloadMissingCovers();
}

void AppController::downloadMissingCovers()
{
    if (m_coverDownload || !m_settings.downloadCovers())
        return;
    const QString coverDir = m_settings.coverCacheDirectory();
    const services::SteamArtCache cache(coverDir);
    // game id -> Steam app ids to try, for games that still have no cover.
    QList<QPair<QString, QStringList>> wanted;
    for (const services::LibraryGame &game : m_library.games()) {
        if (!m_library.art(game.id()).header.isEmpty())
            continue;
        QStringList ids;
        for (const QString &appId : services::steamAppIds(game)) {
            if (!cache.knownMissing(appId))
                ids.append(appId);
        }
        if (!ids.isEmpty())
            wanted.append({game.id(), ids});
    }
    if (wanted.isEmpty())
        return;

    // Runs beside the other operations: covers never make the app busy.
    const QString userAgent = QStringLiteral("SaveVault/") + appVersion();
    auto *watcher = new QFutureWatcher<QHash<QString, QString>>(this);
    m_coverDownload = watcher;
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        m_coverDownload = nullptr;
        const QHash<QString, QString> downloaded = watcher->result();
        if (!downloaded.isEmpty()) {
            QHash<QString, services::GameArt> art;
            for (const services::LibraryGame &game : m_library.games())
                art.insert(game.id(), m_library.art(game.id()));
            for (auto it = downloaded.cbegin(); it != downloaded.cend(); ++it)
                art[it.key()].header = it.value();
            m_library.setArt(art);
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([wanted, coverDir, userAgent] {
        QHash<QString, QString> downloaded;
        auto fetcher = registry::makeFetcher(QString::fromLatin1(services::SteamArtCache::kCdnBase), userAgent);
        if (!fetcher.ok())
            return downloaded;
        services::SteamArtCache covers(coverDir);
        for (const auto &[gameId, appIds] : wanted) {
            for (const QString &appId : appIds) {
                auto cover = covers.download(appId, *fetcher.value());
                if (cover.ok()) {
                    downloaded.insert(gameId, cover.value());
                    break;
                }
                // Offline: stop instead of waiting for every timeout.
                if (cover.error().code == ErrorCode::Network)
                    return downloaded;
            }
        }
        return downloaded;
    }));
}

void AppController::setAutoUpdateRegistry(bool enabled)
{
    if (enabled == m_settings.autoUpdateRegistry())
        return;
    m_settings.setAutoUpdateRegistry(enabled);
    emit registryChanged();
}

QString AppController::registryCheckedText() const
{
    const QDateTime last = m_settings.lastRegistryCheck();
    return last.isValid() ? tr("Última verificação: %1.").arg(relativeTime(last)) : tr("Ainda não verificado.");
}

void AppController::updateRegistry()
{
    runRegistryUpdate(false);
}

void AppController::maybeUpdateRegistry()
{
    // Only the first scan after start-up may trigger an automatic check.
    if (m_autoUpdateChecked)
        return;
    m_autoUpdateChecked = true;
    if (!m_settings.autoUpdateRegistry() || !canUpdateRegistry())
        return;
    const QDateTime last = m_settings.lastRegistryCheck();
    if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < kRegistryCheckIntervalSecs)
        return;
    runRegistryUpdate(true);
}

void AppController::runRegistryUpdate(bool quiet)
{
    const QStringList mirrors = m_settings.registryMirrors();
    if (mirrors.isEmpty()) {
        log(tr("Esta versão não tem um servidor de perfis configurado. Importe uma distribuição de perfis."),
            QStringLiteral("warning"));
        return;
    }
    const QString cacheDir = m_settings.registryCacheDirectory();
    const QString version = appVersion();
    run<RegistryUpdateResult>(
        tr("Procurando novos perfis…"),
        [mirrors, cacheDir, version] {
            RegistryUpdateResult result;
            const auto anchor = registry::builtinTrustAnchor();
            if (!anchor.ok()) {
                result.error = anchor.error().message;
                return result;
            }
            registry::RegistryCache cache(cacheDir);
            registry::UpdateOptions options;
            options.appVersion = version;
            options.userAgent = QStringLiteral("SaveVault/") + version;
            result.channels = registry::updateFromMirrors(mirrors, cache, anchor.value(), options);
            return result;
        },
        [this, quiet](RegistryUpdateResult result) {
            if (!result.error.isEmpty()) {
                log(tr("Não foi possível atualizar os perfis: %1").arg(result.error), QStringLiteral("error"));
                return;
            }
            bool changed = false;
            bool answered = false;
            QStringList failures;
            bool untrusted = false;
            for (const registry::ChannelUpdate &channel : std::as_const(result.channels)) {
                switch (channel.status) {
                case registry::ChannelUpdate::Status::Updated:
                    changed = answered = true;
                    log(tr("Perfis atualizados no canal %1: %2")
                            .arg(channel.channel,
                                 countText(channel.profileCount, tr("perfil disponível"), tr("perfis disponíveis"))),
                        QStringLiteral("success"));
                    break;
                case registry::ChannelUpdate::Status::UpToDate:
                    answered = true;
                    break;
                case registry::ChannelUpdate::Status::Failed:
                    // A channel the mirrors do not publish is not a problem.
                    if (channel.error->code == ErrorCode::NotFound)
                        break;
                    untrusted = untrusted || channel.error->code == ErrorCode::Untrusted;
                    failures.append(channel.channel + QStringLiteral(": ") + channel.error->message);
                    break;
                }
            }
            if (answered) {
                m_settings.setLastRegistryCheck(QDateTime::currentDateTimeUtc());
                emit registryChanged();
            }
            // Automatic checks stay silent when offline; data that fails
            // verification is always reported.
            if (!failures.isEmpty() && (!quiet || untrusted))
                log(tr("Não foi possível atualizar os perfis. Os perfis atuais continuam valendo. (%1)").arg(failures.join(QStringLiteral("; "))),
                    untrusted ? QStringLiteral("error") : QStringLiteral("warning"));
            else if (!changed && !quiet && failures.isEmpty())
                log(tr("Os perfis já estão atualizados."), QStringLiteral("info"));
            if (changed)
                refresh();
        });
}

void AppController::backupGame(const QString &gameId)
{
    runBackup(gameId, m_settings.activeSlot(gameId));
}

void AppController::backupGameAnyway(const QString &gameId)
{
    runBackup(gameId, m_settings.activeSlot(gameId), {}, true);
}

void AppController::runBackup(const QString &gameId, const QString &slot, std::function<void()> done, bool allowLarge)
{
    const services::LibraryGame *game = m_library.game(gameId);
    if (!game || !m_vault) {
        log(m_vault ? tr("Jogo não encontrado.") : tr("O vault não está disponível: %1").arg(m_vaultError),
            QStringLiteral("error"));
        return;
    }
    services::LibraryGame copy = *game;
    vault::Vault *vaultPtr = &*m_vault;
    const services::ServiceEnvironment env = serviceEnvironment();
    run<Result<vault::Snapshot>>(
        tr("Fazendo backup de %1…").arg(copy.target.profile.name),
        [vaultPtr, env, copy, slot, allowLarge, policy = retentionPolicy()] {
            services::BackupOptions options;
            options.slot = slot;
            options.allowLargerThanExpected = allowLarge;
            auto snapshot = services::BackupService(*vaultPtr, env).backup(copy.target, options);
            // Old backups go right away; their space is freed by the weekly clean-up.
            if (snapshot.ok())
                (void)services::applyRetention(*vaultPtr, policy, {copy.id()}, QDateTime::currentDateTimeUtc(), false);
            return snapshot;
        },
        [this, copy, done](Result<vault::Snapshot> snapshot) mutable {
            if (!snapshot.ok() && snapshot.error().code == ErrorCode::TooLarge) {
                const QLocale locale(QLocale::Portuguese, QLocale::Brazil);
                emit largeBackupFound(copy.id(), copy.target.profile.name,
                                      locale.formattedDataSize(snapshot.error().context.toLongLong()),
                                      locale.formattedDataSize(copy.target.profile.backup.maxBytes.value_or(0)));
                return;
            }
            if (!snapshot.ok()) {
                log(tr("Backup de %1 falhou: %2").arg(copy.target.profile.name, snapshot.error().message),
                    QStringLiteral("error"));
                return;
            }
            copy.lastSnapshot = snapshot.value();
            copy.snapshotCount = m_vault ? m_vault->snapshots(copy.id()).size() : copy.snapshotCount + 1;
            m_library.updateGame(copy);
            const bool inconsistent = snapshot.value().consistency == vault::Consistency::Inconsistent;
            log(inconsistent ? tr("Backup de %1 salvo, mas os arquivos mudaram durante a cópia. Feche o jogo e repita.")
                                   .arg(copy.target.profile.name)
                             : tr("Backup de %1 salvo (%2).")
                                   .arg(copy.target.profile.name,
                                        countText(snapshot.value().files.size(), tr("arquivo"), tr("arquivos"))),
                inconsistent ? QStringLiteral("warning") : QStringLiteral("success"));
            if (done)
                done();
        });
}

void AppController::backupAll()
{
    if (!m_vault) {
        log(tr("O vault não está disponível: %1").arg(m_vaultError), QStringLiteral("error"));
        return;
    }
    const QList<services::LibraryGame> games = m_library.games();
    QHash<QString, QString> activeSlots;
    for (const services::LibraryGame &game : games)
        activeSlots.insert(game.id(), m_settings.activeSlot(game.id()));
    vault::Vault *vaultPtr = &*m_vault;
    const services::ServiceEnvironment env = serviceEnvironment();
    run<BackupAllResult>(
        tr("Fazendo backup de todos os jogos…"),
        [vaultPtr, env, games, activeSlots] {
            BackupAllResult result;
            services::BackupService service(*vaultPtr, env);
            for (services::LibraryGame game : games) {
                services::BackupOptions options;
                options.slot = activeSlots.value(game.id());
                auto snapshot = service.backup(game.target, options);
                if (snapshot.ok()) {
                    game.lastSnapshot = snapshot.value();
                    ++game.snapshotCount;
                    result.updated.append(game);
                } else if (snapshot.error().code != ErrorCode::NotFound) {
                    result.failures.append(game.target.profile.name + QStringLiteral(": ") + snapshot.error().message);
                }
            }
            return result;
        },
        [this](BackupAllResult result) {
            for (const services::LibraryGame &game : result.updated)
                m_library.updateGame(game);
            for (const QString &failure : result.failures)
                log(tr("Backup falhou: %1").arg(failure), QStringLiteral("error"));
            log(countText(result.updated.size(), tr("jogo com backup salvo"), tr("jogos com backup salvo")),
                result.failures.isEmpty() ? QStringLiteral("success") : QStringLiteral("warning"));
        });
}

QVariantMap AppController::gameInfo(const QString &gameId) const
{
    const services::LibraryGame *game = m_library.game(gameId);
    if (!game)
        return {};
    const services::GameArt art = m_library.art(gameId);
    const auto fileUrl = [](const QString &path) { return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path); };
    // [{url, path}] with the path in the OS's notation, for display.
    const auto folderList = [](const QStringList &folders) {
        QVariantList list;
        for (const QString &folder : folders)
            list.append(QVariantMap{{QStringLiteral("url"), QUrl::fromLocalFile(folder)},
                                    {QStringLiteral("path"), QDir::toNativeSeparators(folder)}});
        return list;
    };
    return {{QStringLiteral("name"), game->target.profile.name},
            {QStringLiteral("store"), storeLabel(*game)},
            {QStringLiteral("reason"), game->install || game->saveFilesFound > 0
                                           ? reasonLabel(game->reason, game->install.has_value())
                                           : tr("Não encontrado nesta máquina; os backups continuam disponíveis")},
            {QStringLiteral("installDir"), game->install ? game->install->installDir : QString()},
            {QStringLiteral("launcherCloud"), game->launcherCloud},
            {QStringLiteral("snapshotCount"), static_cast<int>(game->snapshotCount)},
            {QStringLiteral("coverColor"), coverColor(gameId)},
            {QStringLiteral("profileSource"), game->target.channel},
            {QStringLiteral("local"), game->target.channel == u"local"},
            {QStringLiteral("cover"), fileUrl(art.header.isEmpty() ? art.icon : art.header)},
            {QStringLiteral("iconOnly"), art.header.isEmpty() && !art.icon.isEmpty()},
            {QStringLiteral("hero"), fileUrl(art.hero)},
            {QStringLiteral("logo"), fileUrl(art.logo)},
            {QStringLiteral("saveFolders"), folderList(services::saveFolders(game->target))}};
}

QVariantList AppController::snapshots(const QString &gameId) const
{
    QVariantList list;
    if (!m_vault)
        return list;
    const QList<vault::Snapshot> all = m_vault->snapshots(gameId);
    for (auto it = all.crbegin(); it != all.crend(); ++it) {
        list.append(QVariantMap{
            {QStringLiteral("id"), uuidToString(it->snapshotId)},
            {QStringLiteral("date"), it->createdAt.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))},
            {QStringLiteral("relative"), relativeTime(it->createdAt)},
            {QStringLiteral("kind"), static_cast<int>(it->kind)},
            {QStringLiteral("kindText"), snapshotKindLabel(it->kind)},
            {QStringLiteral("files"), static_cast<int>(it->files.size())},
            {QStringLiteral("label"), it->label},
            {QStringLiteral("slot"), it->slot},
            {QStringLiteral("pinned"), it->pinned},
            {QStringLiteral("inconsistent"), it->consistency == vault::Consistency::Inconsistent}});
    }
    return list;
}

QVariantList AppController::saveSlots(const QString &gameId) const
{
    QVariantList list;
    if (!m_vault)
        return list;
    const QString active = m_settings.activeSlot(gameId);
    for (const services::SaveSlot &slot : services::saveSlots(*m_vault, gameId)) {
        list.append(QVariantMap{
            {QStringLiteral("name"), slot.name},
            {QStringLiteral("active"), slot.name == active},
            {QStringLiteral("snapshotId"), uuidToString(slot.head.snapshotId)},
            {QStringLiteral("date"), slot.head.createdAt.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))},
            {QStringLiteral("relative"), relativeTime(slot.head.createdAt)},
            {QStringLiteral("snapshots"), static_cast<int>(slot.snapshots)}});
    }
    return list;
}

QString AppController::activeSlot(const QString &gameId) const
{
    return m_settings.activeSlot(gameId);
}

QString AppController::saveAsSlot(const QString &gameId, const QString &name)
{
    const QString slot = name.simplified();
    if (!vault::isValidSlotName(slot))
        return tr("Use um nome de 1 a 100 caracteres.");
    if (!m_vault)
        return tr("O vault não está disponível: %1").arg(m_vaultError);
    for (const services::SaveSlot &existing : services::saveSlots(*m_vault, gameId)) {
        if (existing.name == slot)
            return tr("Já existe um save com esse nome.");
    }
    if (busy())
        return tr("Aguarde a operação atual terminar.");
    runBackup(gameId, slot, [this, gameId, slot] {
        m_settings.setActiveSlot(gameId, slot);
        emit slotsChanged(gameId);
    });
    return {};
}

QVariantMap AppController::planRestore(const QString &gameId, const QString &snapshotId)
{
    m_plan.reset();
    const services::LibraryGame *game = m_library.game(gameId);
    if (!game || !m_vault)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), tr("Jogo ou vault indisponível.")}};

    auto plan = services::RestoreService(*m_vault, serviceEnvironment()).plan(game->target, QUuid::fromString(snapshotId));
    if (!plan.ok())
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), plan.error().message}};

    QVariantList files;
    for (const services::PlannedFile &file : plan.value().files) {
        const QFileInfo current(file.target);
        const QLocale locale(QLocale::Portuguese, QLocale::Brazil);
        files.append(QVariantMap{{QStringLiteral("target"), file.target.isEmpty() ? file.source.path : file.target},
                                 // Shortest readable form: the file and its folder.
                                 {QStringLiteral("name"), file.source.path.section(u'/', -2)},
                                 {QStringLiteral("action"), actionName(file.action)},
                                 {QStringLiteral("reason"), file.skipReason},
                                 {QStringLiteral("newer"), file.currentIsNewer},
                                 {QStringLiteral("backupSize"), locale.formattedDataSize(file.source.size)},
                                 {QStringLiteral("currentSize"), current.exists() ? locale.formattedDataSize(current.size()) : QString()}});
    }
    const services::RestorePlan &p = plan.value();
    QVariantMap result{{QStringLiteral("ok"), true},
                       {QStringLiteral("create"), static_cast<int>(p.count(services::FileAction::Create))},
                       {QStringLiteral("overwrite"), static_cast<int>(p.count(services::FileAction::Overwrite))},
                       {QStringLiteral("unchanged"), static_cast<int>(p.count(services::FileAction::Unchanged))},
                       {QStringLiteral("skip"), static_cast<int>(p.count(services::FileAction::Skip))},
                       {QStringLiteral("newer"), p.hasNewerLocalFiles()},
                       {QStringLiteral("extra"), static_cast<int>(p.extraFiles.size())},
                       {QStringLiteral("cloudConflict"), p.launcherCloudConflict},
                       {QStringLiteral("gameRunning"), p.gameRunning},
                       {QStringLiteral("hasChanges"), p.hasChanges()},
                       {QStringLiteral("slot"), p.snapshot.slot},
                       {QStringLiteral("registry"), static_cast<int>(p.registryKeys.size())},
                       {QStringLiteral("registrySkipped"), static_cast<int>(p.skippedRegistryKeys)},
                       {QStringLiteral("activeSlot"), m_settings.activeSlot(gameId)},
                       {QStringLiteral("files"), files}};
    m_plan = std::move(plan).value();
    m_planGameId = gameId;
    return result;
}

void AppController::restore(const QString &gameId, const QString &snapshotId, const QString &policy)
{
    const services::LibraryGame *game = m_library.game(gameId);
    if (!game || !m_vault || !m_plan || m_planGameId != gameId
        || uuidToString(m_plan->snapshot.snapshotId) != QUuid::fromString(snapshotId).toString(QUuid::WithoutBraces).toLower()) {
        log(tr("Revise o plano de restauração antes de confirmar."), QStringLiteral("warning"));
        return;
    }
    const services::RestorePlan plan = *m_plan;
    m_plan.reset();
    services::LibraryGame copy = *game;
    vault::Vault *vaultPtr = &*m_vault;
    const services::ServiceEnvironment env = serviceEnvironment();
    const services::CloudPolicy cloud = toPolicy(policy);
    const QString currentSlot = m_settings.activeSlot(gameId);
    const QString restoredSlot = plan.snapshot.slot;
    run<Result<services::RestoreReport>>(
        tr("Restaurando %1…").arg(copy.target.profile.name),
        [vaultPtr, env, copy, plan, cloud, currentSlot] {
            return services::RestoreService(*vaultPtr, env).execute(copy.target, plan, cloud, currentSlot);
        },
        [this, copy, currentSlot, restoredSlot](Result<services::RestoreReport> report) mutable {
            if (!report.ok()) {
                log(tr("Restauração de %1 não foi feita: %2").arg(copy.target.profile.name, report.error().message),
                    QStringLiteral("error"));
                return;
            }
            if (report.value().preRestoreSnapshotId)
                ++copy.snapshotCount;
            // The files on disk now belong to the restored snapshot's slot.
            m_settings.setActiveSlot(copy.id(), restoredSlot);
            m_library.updateGame(copy);
            emit slotsChanged(copy.id());
            if (restoredSlot != currentSlot && !restoredSlot.isEmpty())
                log(tr("Agora você está no save \u201c%1\u201d de %2.").arg(restoredSlot, copy.target.profile.name),
                    QStringLiteral("success"));
            else
                log(tr("%1 restaurado (%2). Os arquivos anteriores estão no histórico como \"Antes de restaurar\".")
                        .arg(copy.target.profile.name, countText(report.value().written, tr("arquivo"), tr("arquivos"))),
                    QStringLiteral("success"));
        });
}

QVariantList AppController::exportableGames() const
{
    QVariantList list;
    for (const services::LibraryGame &game : m_library.games()) {
        if (game.snapshotCount == 0)
            continue;
        const services::GameArt art = m_library.art(game.id());
        list.append(QVariantMap{{QStringLiteral("id"), game.id()},
                                {QStringLiteral("name"), game.target.profile.name},
                                {QStringLiteral("snapshots"), static_cast<int>(game.snapshotCount)},
                                {QStringLiteral("cover"), art.header.isEmpty() ? QUrl() : QUrl::fromLocalFile(art.header)},
                                {QStringLiteral("coverColor"), coverColor(game.id())}});
    }
    return list;
}

QVariantMap AppController::lastExport() const
{
    const auto record = services::ExportLog(m_settings.exportLogDirectory()).latest();
    if (!record)
        return {{QStringLiteral("exists"), false}};
    return {{QStringLiteral("exists"), true},
            {QStringLiteral("date"), record->createdAt.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))},
            {QStringLiteral("games"), static_cast<int>(record->gameIds.size())}};
}

void AppController::exportGames(const QStringList &gameIds, const QUrl &file, bool allHistory, bool incremental,
                                bool includeUserFolders)
{
    if (!m_vault) {
        log(tr("O vault não está disponível: %1").arg(m_vaultError), QStringLiteral("error"));
        return;
    }
    QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (path.isEmpty() || gameIds.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(archive::kGamesaveExtension), Qt::CaseInsensitive))
        path += QLatin1String(archive::kGamesaveExtension);

    services::ExportRequest request;
    request.path = path;
    request.allHistory = allHistory;
    request.appVersion = appVersion();
    request.sourceOs = platform::osName(platform::hostOs());
    const services::LocalProfileStore local(m_settings.localProfilesDirectory());
    for (const QString &id : gameIds) {
        const services::LibraryGame *game = m_library.game(id);
        services::ExportGame entry{id, game ? game->target.profile.name : id, std::nullopt};
        if (game && game->target.channel == u"local") {
            if (auto json = local.profileJson(id); json.ok())
                entry.localProfile = QJsonDocument::fromJson(json.value()).object();
        }
        request.games.append(entry);
    }
    services::ExportLog exportLog(m_settings.exportLogDirectory());
    if (incremental)
        request.base = exportLog.latest();
    if (includeUserFolders) {
        const platform::FolderMap folders = detection::DetectionEnvironment::host().userFolders;
        for (const QString &name : folders.placeholders())
            request.placeholders.insert(u'<' + name + u'>', folders.value(name));
    }

    const vault::Vault *vaultPtr = &*m_vault;
    const QString logDir = m_settings.exportLogDirectory();
    run<Result<services::ExportRecord>>(
        tr("Exportando…"),
        [vaultPtr, request, logDir] {
            auto record = services::exportGames(*vaultPtr, request);
            if (record.ok())
                (void)services::ExportLog(logDir).add(record.value());
            return record;
        },
        [this, path](Result<services::ExportRecord> record) {
            if (!record.ok()) {
                log(tr("A exportação falhou: %1").arg(record.error().message), QStringLiteral("error"));
                return;
            }
            log(tr("%1 exportado para %2.")
                    .arg(countText(record.value().gameIds.size(), tr("jogo"), tr("jogos")), QDir::toNativeSeparators(path)),
                QStringLiteral("success"));
        });
}

QVariantMap AppController::inspectGamesave(const QUrl &file) const
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const auto reader = archive::GamesaveReader::open(path);
    if (!reader.ok())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), reader.error().code == ErrorCode::Unsupported
                                              ? tr("Este arquivo foi criado por uma versão mais nova do SaveVault. Atualize o app para abri-lo.")
                                              : tr("O arquivo não é um .gamesave válido ou está danificado.")}};
    const archive::GamesaveManifest &manifest = reader.value().manifest();
    QVariantList games;
    for (const archive::GamesaveGame &game : manifest.games)
        games.append(QVariantMap{{QStringLiteral("name"), game.name},
                                 {QStringLiteral("snapshots"), static_cast<int>(game.snapshots.size())}});
    bool missingBase = false;
    for (const QString &blob : reader.value().missingBlobs()) {
        if (!m_vault || !m_vault->blobs().contains(blob)) {
            missingBase = true;
            break;
        }
    }
    return {{QStringLiteral("ok"), true},
            {QStringLiteral("games"), games},
            {QStringLiteral("incremental"), manifest.incremental()},
            {QStringLiteral("missingBase"), missingBase},
            {QStringLiteral("date"), manifest.createdAt.toLocalTime().toString(QStringLiteral("dd/MM/yyyy HH:mm"))},
            {QStringLiteral("sourceOs"), manifest.sourceOs}};
}

void AppController::importGamesave(const QUrl &file)
{
    if (!m_vault) {
        log(tr("O vault não está disponível: %1").arg(m_vaultError), QStringLiteral("error"));
        return;
    }
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QSet<QString> known;
    for (const services::LibraryGame &game : m_library.games())
        known.insert(game.id());
    vault::Vault *vaultPtr = &*m_vault;
    const QString localDir = m_settings.localProfilesDirectory();
    run<Result<services::ImportReport>>(
        tr("Importando…"),
        [vaultPtr, path, localDir, known]() -> Result<services::ImportReport> {
            const auto reader = archive::GamesaveReader::open(path);
            if (!reader.ok())
                return reader.error();
            services::LocalProfileStore local(localDir);
            return services::importGamesave(*vaultPtr, reader.value(), &local, known);
        },
        [this](Result<services::ImportReport> report) {
            if (!report.ok()) {
                log(tr("A importação falhou: %1").arg(report.error().message), QStringLiteral("error"));
                return;
            }
            log(tr("%1 importado de %2. Nada foi restaurado: abra o jogo na biblioteca para escolher um backup.")
                    .arg(countText(report.value().imported, tr("backup"), tr("backups")),
                         report.value().games.join(QStringLiteral(", "))),
                QStringLiteral("success"));
            refresh();
        });
}

QString AppController::addManualGame(const QString &name, const QVariantList &folders)
{
    if (name.simplified().isEmpty())
        return tr("Digite o nome do jogo.");
    QStringList paths;
    for (const QVariant &folder : folders) {
        const QUrl url = folder.toUrl();
        paths.append(url.isLocalFile() ? url.toLocalFile() : folder.toString());
    }
    paths.removeAll(QString());
    if (paths.isEmpty())
        return tr("Escolha pelo menos uma pasta de saves.");

    services::LocalProfileStore store(m_settings.localProfilesDirectory());
    const auto added = store.addManualGame(name, paths, detection::DetectionEnvironment::host().userFolders);
    if (!added.ok()) {
        const Error &error = added.error();
        if (error.message.startsWith(QLatin1String("the folder is outside")))
            return tr("A pasta %1 fica fora das suas pastas de usuário (pasta pessoal, Documentos, AppData). "
                      "Por enquanto, os jogos adicionados à mão precisam guardar os saves nessas pastas.")
                .arg(QDir::toNativeSeparators(error.context));
        if (error.context.startsWith(QLatin1String("/saves")))
            return tr("Essa pasta é ampla demais. Escolha a pasta do jogo, e não uma pasta inteira do sistema.");
        return tr("Não foi possível adicionar o jogo: %1").arg(error.message);
    }
    log(tr("%1 adicionado à biblioteca").arg(added.value().name), QStringLiteral("success"));
    refresh();
    return {};
}

QString AppController::importProfile(const QUrl &file)
{
    services::LocalProfileStore store(m_settings.localProfilesDirectory());
    const auto imported = store.importProfile(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!imported.ok())
        return tr("O arquivo não é um perfil válido: %1").arg(imported.error().message);
    log(tr("Perfil de %1 importado").arg(imported.value().name), QStringLiteral("success"));
    refresh();
    return {};
}

void AppController::removeLocalGame(const QString &gameId)
{
    const services::LibraryGame *game = m_library.game(gameId);
    if (!game || game->target.channel != u"local")
        return;
    const QString name = game->target.profile.name;
    services::LocalProfileStore store(m_settings.localProfilesDirectory());
    if (auto removed = store.remove(gameId); !removed.ok()) {
        log(tr("Não foi possível remover %1: %2").arg(name, removed.error().message), QStringLiteral("error"));
        return;
    }
    log(tr("%1 removido. Os backups continuam no vault.").arg(name), QStringLiteral("info"));
    refresh();
}

void AppController::importRegistry(const QUrl &folder)
{
    const QString source = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    const QString cacheDir = m_settings.registryCacheDirectory();
    const QString version = appVersion();
    run<QStringList>(
        tr("Verificando perfis…"),
        [source, cacheDir, version] {
            QStringList messages;
            const auto anchor = registry::builtinTrustAnchor();
            if (!anchor.ok())
                return QStringList{QStringLiteral("error:") + anchor.error().message};
            registry::RegistryCache cache(cacheDir);
            const QStringList channels =
                QDir(source + QStringLiteral("/channels")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            if (channels.isEmpty())
                return QStringList{QStringLiteral("error:") + tr("A pasta escolhida não contém uma distribuição de perfis.")};
            for (const QString &channel : channels) {
                const auto imported = cache.importChannel(source, channel, anchor.value(), version);
                messages.append(imported.ok()
                                    ? QStringLiteral("success:") + tr("%1: %2").arg(channel, countText(imported.value().profiles.size(), tr("perfil verificado"), tr("perfis verificados")))
                                    : QStringLiteral("error:") + channel + QStringLiteral(": ") + imported.error().message);
            }
            return messages;
        },
        [this](QStringList messages) {
            for (const QString &message : messages)
                log(message.section(u':', 1), message.section(u':', 0, 0));
            refresh();
        });
}

QString AppController::setVaultFolder(const QUrl &folder)
{
    const QString path = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    auto opened = vault::Vault::open(path);
    if (!opened.ok())
        return opened.error().message;
    m_settings.setVaultPath(path);
    openVault();
    log(tr("Vault alterado para %1").arg(QDir::toNativeSeparators(path)), QStringLiteral("info"));
    refresh();
    return {};
}

void AppController::setCloudPolicy(const QString &policy)
{
    if (policy == cloudPolicy())
        return;
    m_settings.setCloudPolicy(policy);
    emit cloudPolicyChanged();
}

} // namespace savevault::backend
