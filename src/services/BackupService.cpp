#include "saveforge/services/BackupService.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBackup, "saveforge.backup")

namespace saveforge::services {

BackupService::BackupService(vault::Vault &vault, ServiceEnvironment env)
    : m_vault(vault)
    , m_env(std::move(env))
{
}

Result<vault::Snapshot> BackupService::backup(const GameTarget &target, const BackupOptions &options)
{
    if (target.profile.backup.requireGameClosed && isGameRunning(target, m_env.processes()))
        return Error{ErrorCode::PermissionDenied, QStringLiteral("close the game before backing it up"),
                     target.profile.id};

    const QList<profiles::ResolvedFile> files = profiles::PathResolver().resolve(target.profile, target.context);
    if (files.isEmpty())
        return Error{ErrorCode::NotFound, QStringLiteral("no save files found for this game"), target.profile.id};

    vault::SnapshotRequest request;
    request.gameId = target.effectiveGameId();
    request.kind = options.kind;
    request.trigger = options.trigger;
    request.label = options.label;
    request.machineId = m_env.machineId;
    request.appVersion = m_env.appVersion;
    request.profile = {target.channel, target.profile.id, target.profile.revision};
    for (const profiles::ResolvedFile &file : files)
        request.files.append({file.absolutePath, file.templatePath, file.ruleId, file.bindings});

    auto snapshot = m_vault.createSnapshot(request, options.onFileRead);
    if (snapshot.ok())
        qCInfo(lcBackup) << "backed up" << request.gameId << "-" << snapshot.value().files.size() << "file(s)";
    return snapshot;
}

} // namespace saveforge::services
