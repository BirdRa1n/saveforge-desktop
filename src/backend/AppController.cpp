#include "AppController.h"

#include "Formatting.h"

#include "saveforge/core/Uuid.h"
#include "saveforge/core/Version.h"
#include "saveforge/detection/Launchers.h"
#include "saveforge/registry/RegistryCache.h"
#include "saveforge/registry/RegistryUpdater.h"
#include "saveforge/registry/Fetcher.h"
#include "saveforge/services/BackupService.h"
#include "saveforge/services/GameArt.h"
#include "saveforge/services/LocalProfiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QTime>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace saveforge::backend {

namespace {

constexpr qsizetype kMaxActivity = 200;

struct RefreshResult {
    services::CatalogLoad catalog;
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

// Automatic checks run at most this often.
constexpr qint64 kRegistryCheckIntervalSecs = 24 * 60 * 60;

struct BackupAllResult {
    QList<services::LibraryGame> updated;
    QStringList failures;
};

services::CloudPolicy toPolicy(const QString &policy)
{
    if (policy == u"prefer-saveforge")
        return services::CloudPolicy::PreferSaveForge;
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
            result.catalog = services::loadCatalogFromCache(cacheDir, version, localDir);
            const detection::DetectionEnvironment env = detection::DetectionEnvironment::host();
            result.scan = services::scanLibrary(result.catalog.catalog, detection::detectInstalledGames(env), env, vaultPtr);
            result.art = findArt(result.scan.games, env, coverDir);
            return result;
        },
        [this](RefreshResult result) {
            m_profileCount = static_cast<int>(result.catalog.catalog.size());
            m_usingTestKeys = result.catalog.usingTestKeys;
            m_warnings = result.catalog.warnings;
            m_notices.clear();
            for (const registry::Notice &notice : result.catalog.notices)
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
    const QString userAgent = QStringLiteral("SaveForge/") + appVersion();
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
            options.userAgent = QStringLiteral("SaveForge/") + version;
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
        [vaultPtr, env, copy] { return services::BackupService(*vaultPtr, env).backup(copy.target); },
        [this, copy](Result<vault::Snapshot> snapshot) mutable {
            if (!snapshot.ok()) {
                log(tr("Backup de %1 falhou: %2").arg(copy.target.profile.name, snapshot.error().message),
                    QStringLiteral("error"));
                return;
            }
            copy.lastSnapshot = snapshot.value();
            ++copy.snapshotCount;
            m_library.updateGame(copy);
            const bool inconsistent = snapshot.value().consistency == vault::Consistency::Inconsistent;
            log(inconsistent ? tr("Backup de %1 salvo, mas os arquivos mudaram durante a cópia. Feche o jogo e repita.")
                                   .arg(copy.target.profile.name)
                             : tr("Backup de %1 salvo (%2).")
                                   .arg(copy.target.profile.name,
                                        countText(snapshot.value().files.size(), tr("arquivo"), tr("arquivos"))),
                inconsistent ? QStringLiteral("warning") : QStringLiteral("success"));
        });
}

void AppController::backupAll()
{
    if (!m_vault) {
        log(tr("O vault não está disponível: %1").arg(m_vaultError), QStringLiteral("error"));
        return;
    }
    const QList<services::LibraryGame> games = m_library.games();
    vault::Vault *vaultPtr = &*m_vault;
    const services::ServiceEnvironment env = serviceEnvironment();
    run<BackupAllResult>(
        tr("Fazendo backup de todos os jogos…"),
        [vaultPtr, env, games] {
            BackupAllResult result;
            services::BackupService service(*vaultPtr, env);
            for (services::LibraryGame game : games) {
                auto snapshot = service.backup(game.target);
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
            {QStringLiteral("logo"), fileUrl(art.logo)}};
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
            {QStringLiteral("inconsistent"), it->consistency == vault::Consistency::Inconsistent}});
    }
    return list;
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
        files.append(QVariantMap{{QStringLiteral("target"), file.target.isEmpty() ? file.source.path : file.target},
                                 {QStringLiteral("action"), actionName(file.action)},
                                 {QStringLiteral("reason"), file.skipReason},
                                 {QStringLiteral("newer"), file.currentIsNewer}});
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
    run<Result<services::RestoreReport>>(
        tr("Restaurando %1…").arg(copy.target.profile.name),
        [vaultPtr, env, copy, plan, cloud] {
            return services::RestoreService(*vaultPtr, env).execute(copy.target, plan, cloud);
        },
        [this, copy](Result<services::RestoreReport> report) mutable {
            if (!report.ok()) {
                log(tr("Restauração de %1 não foi feita: %2").arg(copy.target.profile.name, report.error().message),
                    QStringLiteral("error"));
                return;
            }
            if (report.value().preRestoreSnapshotId)
                ++copy.snapshotCount;
            m_library.updateGame(copy);
            log(tr("%1 restaurado (%2). Os arquivos anteriores estão no histórico como \"Antes de restaurar\".")
                    .arg(copy.target.profile.name, countText(report.value().written, tr("arquivo"), tr("arquivos"))),
                QStringLiteral("success"));
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

} // namespace saveforge::backend
