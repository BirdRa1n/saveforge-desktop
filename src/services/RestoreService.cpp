#include "savevault/services/RestoreService.h"

#include "savevault/core/Uuid.h"
#include "savevault/platform/FileSync.h"
#include "savevault/services/BackupService.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QHash>
#include <QJsonValue>
#include <QSet>

#include <algorithm>
#include <QTimeZone>

Q_LOGGING_CATEGORY(lcRestore, "savevault.restore")

namespace savevault::services {

namespace {

QString sha256Of(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

bool launcherCloudConflict(const GameTarget &target)
{
    const QStringList &stores = target.profile.backup.cloudSync;
    return !stores.isEmpty() && (target.context.store.isEmpty() || stores.contains(target.context.store));
}

// A file written next to its target, waiting to be swapped in.
struct StagedFile {
    QString target;
    QString staged;
    QString previous;      // where the old file was moved, if any
    bool committed = false;
};

QString sibling(const QString &target, const QString &id, const char *suffix)
{
    return target + QStringLiteral(".savevault-") + id + QLatin1String(suffix);
}

} // namespace

qsizetype RestorePlan::count(FileAction action) const
{
    return std::count_if(files.begin(), files.end(), [action](const PlannedFile &f) { return f.action == action; });
}

bool RestorePlan::hasNewerLocalFiles() const
{
    return std::any_of(files.begin(), files.end(), [](const PlannedFile &f) { return f.currentIsNewer; });
}

RestoreService::RestoreService(vault::Vault &vault, ServiceEnvironment env)
    : m_vault(vault)
    , m_env(std::move(env))
{
}

Result<RestorePlan> RestoreService::plan(const GameTarget &target, const QUuid &snapshotId,
                                         const QHash<QString, QString> &bindingOverrides) const
{
    auto snapshot = m_vault.snapshot(target.effectiveGameId(), snapshotId);
    if (!snapshot.ok())
        return snapshot.error();

    RestorePlan plan;
    plan.snapshot = std::move(snapshot).value();
    plan.launcherCloudConflict = launcherCloudConflict(target);
    plan.gameRunning = isGameRunning(target, m_env.processes());

    // Paths are compared the way the file system of this game compares them.
    const auto key = [&target](const QString &path) {
        const QString clean = QDir::cleanPath(path);
        return target.context.caseInsensitive ? clean.toLower() : clean;
    };
    QSet<QString> targets;
    for (const vault::SnapshotFile &file : plan.snapshot.files) {
        PlannedFile planned;
        planned.source = file;

        QHash<QString, QString> bindings = file.bindings;
        for (auto it = bindingOverrides.cbegin(); it != bindingOverrides.cend(); ++it)
            bindings.insert(it.key(), it.value());

        const auto resolved = profiles::resolveRestoreTarget(file.path, bindings, target.context);
        if (!resolved.ok()) {
            planned.skipReason = resolved.error().message;
        } else {
            planned.target = resolved.value();
            const QFileInfo current(planned.target);
            if (!current.exists()) {
                planned.action = FileAction::Create;
            } else if (!current.isFile()) {
                planned.action = FileAction::Skip;
                planned.skipReason = QStringLiteral("a folder exists where the file should be");
            } else {
                planned.action = sha256Of(planned.target) == file.sha256 ? FileAction::Unchanged : FileAction::Overwrite;
                planned.currentIsNewer = planned.action == FileAction::Overwrite
                                         && current.lastModified(QTimeZone::UTC) > plan.snapshot.createdAt;
            }
            targets.insert(key(planned.target));
        }
        plan.files.append(planned);
    }

    for (const profiles::ResolvedFile &current : profiles::PathResolver().resolve(target.profile, target.context)) {
        if (!targets.contains(key(current.absolutePath)))
            plan.extraFiles.append(current.absolutePath);
    }

    // Registry keys whose values differ from what the snapshot holds.
    if (!plan.snapshot.registry.isEmpty()) {
        if (!registryRulesApply(target, m_env)) {
            plan.skippedRegistryKeys = plan.snapshot.registry.size();
        } else {
            for (const vault::RegistryKey &saved : plan.snapshot.registry) {
                if (!platform::isUserSoftwareKey(saved.key)) {
                    ++plan.skippedRegistryKeys;
                    continue;
                }
                auto current = m_env.registry->readTree(saved.key);
                QHash<QString, QPair<QString, QJsonValue>> now;
                if (current.ok() && !current.value().isEmpty()
                    && current.value().first().key.compare(saved.key, Qt::CaseInsensitive) == 0) {
                    for (const platform::RegistryValueData &v : current.value().first().values)
                        now.insert(v.name.toLower(), {v.type, v.data});
                }
                const bool differs = std::any_of(saved.values.cbegin(), saved.values.cend(), [&now](const vault::RegistryValue &v) {
                    const auto it = now.constFind(v.name.toLower());
                    return it == now.cend() || it->first != v.type || it->second != v.data;
                });
                if (differs || (saved.values.isEmpty() && (!current.ok() || current.value().isEmpty())))
                    plan.registryKeys.append(saved);
            }
        }
    }
    return plan;
}

namespace {

// Writes the given keys; on failure, writes `previous` back (best effort).
Result<void> writeRegistry(platform::RegistryStore &store, const QList<vault::RegistryKey> &keys,
                           const QList<vault::RegistryKey> &previous)
{
    for (const vault::RegistryKey &key : keys) {
        platform::RegistryKeyData data{key.key, {}};
        for (const vault::RegistryValue &value : key.values)
            data.values.append({value.name, value.type, value.data});
        if (auto written = store.writeValues(data); !written.ok()) {
            for (const vault::RegistryKey &old : previous) {
                platform::RegistryKeyData back{old.key, {}};
                for (const vault::RegistryValue &value : old.values)
                    back.values.append({value.name, value.type, value.data});
                (void)store.writeValues(back);
            }
            return written.error();
        }
    }
    return {};
}

} // namespace

Result<RestoreReport> RestoreService::execute(const GameTarget &target, const RestorePlan &plan, CloudPolicy policy,
                                              const QString &currentSlot)
{
    if (isGameRunning(target, m_env.processes()))
        return Error{ErrorCode::PermissionDenied, QStringLiteral("close the game before restoring"), target.profile.id};
    if (plan.launcherCloudConflict && policy == CloudPolicy::PreferLauncher)
        return Error{ErrorCode::PermissionDenied,
                     QStringLiteral("the launcher's cloud has priority for this game; restore manually if needed"),
                     target.profile.id};
    if (plan.launcherCloudConflict && policy == CloudPolicy::Ask)
        return Error{ErrorCode::InvalidArgument,
                     QStringLiteral("choose whether the launcher's cloud or SaveVault has priority"), target.profile.id};

    RestoreReport report;
    report.unchanged = plan.count(FileAction::Unchanged);
    report.skipped = plan.count(FileAction::Skip);
    if (!plan.hasChanges())
        return report;

    // 1. Undo point: capture what is on disk now.
    BackupOptions undo;
    undo.kind = vault::SnapshotKind::PreRestore;
    undo.slot = currentSlot;
    auto preRestore = BackupService(m_vault, m_env).backup(target, undo);
    if (preRestore.ok())
        report.preRestoreSnapshotId = preRestore.value().snapshotId;
    else if (preRestore.error().code != ErrorCode::NotFound) // nothing to protect is fine
        return Error{preRestore.error().code,
                     QStringLiteral("could not save the current files before restoring: ") + preRestore.error().message,
                     preRestore.error().context};

    // 2. Stage every file next to its target and verify it while writing.
    const QString id = uuidToString(createUuidV7()).left(8);
    QList<StagedFile> staged;
    const auto discardStaged = [&staged] {
        for (const StagedFile &s : std::as_const(staged))
            QFile::remove(s.staged);
    };
    for (const PlannedFile &file : plan.files) {
        if (file.action != FileAction::Create && file.action != FileAction::Overwrite)
            continue;
        const QString dir = QFileInfo(file.target).absolutePath();
        StagedFile s{file.target, sibling(file.target, id, ".tmp"), {}, false};
        QFile out(s.staged);
        if (!QDir().mkpath(dir) || !out.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            discardStaged();
            return Error{ErrorCode::Io, QStringLiteral("cannot write next to the save file"), file.target};
        }
        staged.append(s);
        auto written = m_vault.blobs().read(file.source.sha256, out);
        if (!written.ok() || !platform::syncFile(out)) {
            out.close();
            discardStaged();
            return written.ok() ? Error{ErrorCode::Io, QStringLiteral("cannot persist restored file"), file.target}
                                : written.error();
        }
        out.setFileTime(file.source.mtime, QFileDevice::FileModificationTime);
        out.close();
    }

    // 3. Registry values first: unlike files, they cannot be staged, so a
    //    failure here leaves the files untouched and puts the old values back.
    const QList<vault::RegistryKey> previousRegistry =
        preRestore.ok() ? preRestore.value().registry : QList<vault::RegistryKey>{};
    if (!plan.registryKeys.isEmpty()) {
        if (auto written = writeRegistry(*m_env.registry, plan.registryKeys, previousRegistry); !written.ok()) {
            discardStaged();
            return Error{written.error().code,
                         QStringLiteral("cannot restore the registry; nothing was changed: ") + written.error().message,
                         written.error().context};
        }
    }

    // 4. Swap in: move each current file aside, then move the staged file in.
    for (StagedFile &s : staged) {
        bool ok = true;
        if (QFileInfo::exists(s.target)) {
            s.previous = sibling(s.target, id, ".old");
            ok = QFile::rename(s.target, s.previous);
        }
        ok = ok && QFile::rename(s.staged, s.target);
        if (!ok) {
            // Roll back everything committed so far, newest first.
            for (auto it = staged.rbegin(); it != staged.rend(); ++it) {
                if (it->committed)
                    QFile::remove(it->target);
                if (!it->previous.isEmpty() && QFileInfo::exists(it->previous))
                    QFile::rename(it->previous, it->target);
                QFile::remove(it->staged);
            }
            if (!plan.registryKeys.isEmpty())
                (void)writeRegistry(*m_env.registry, previousRegistry, {});
            return Error{ErrorCode::Io, QStringLiteral("restore failed; the previous files were put back"), s.target};
        }
        s.committed = true;
    }

    // 5. Done: drop the moved-aside files and persist directory entries.
    QSet<QString> dirs;
    for (const StagedFile &s : std::as_const(staged)) {
        if (!s.previous.isEmpty())
            QFile::remove(s.previous);
        dirs.insert(QFileInfo(s.target).absolutePath());
    }
    for (const QString &dir : std::as_const(dirs))
        platform::syncDirectory(dir);

    report.written = staged.size() + plan.registryKeys.size();
    qCInfo(lcRestore) << "restored" << report.written << "file(s) of" << target.effectiveGameId() << "from snapshot"
                      << uuidToString(plan.snapshot.snapshotId);
    return report;
}

} // namespace savevault::services
