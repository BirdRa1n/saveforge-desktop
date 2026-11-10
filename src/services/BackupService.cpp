#include "savevault/services/BackupService.h"

#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <algorithm>

Q_LOGGING_CATEGORY(lcBackup, "savevault.backup")

namespace savevault::services {

BackupService::BackupService(vault::Vault &vault, ServiceEnvironment env)
    : m_vault(vault)
    , m_env(std::move(env))
{
}

namespace {

// Whether a file matches a consistentGroups glob, with the rules of `exclude`
// patterns (profile/SPEC.md section 4.1): a pattern with "/" is matched
// against the path below the root placeholder, one without against the name.
bool matchesGroupPattern(const QString &templatePath, const QString &pattern, Qt::CaseSensitivity cs)
{
    const QString relative = templatePath.section(u'/', 1);
    const QString subject = pattern.contains(u'/') ? relative : templatePath.section(u'/', -1);
    const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern, QRegularExpression::NonPathWildcardConversion),
                                cs == Qt::CaseInsensitive ? QRegularExpression::CaseInsensitiveOption
                                                          : QRegularExpression::NoPatternOption);
    return re.match(subject).hasMatch();
}

QList<QList<qsizetype>> consistentGroups(const GameTarget &target, const QList<profiles::ResolvedFile> &files)
{
    const Qt::CaseSensitivity cs = target.context.caseInsensitive ? Qt::CaseInsensitive : Qt::CaseSensitive;
    QList<QList<qsizetype>> groups;
    for (const QStringList &patterns : target.profile.backup.consistentGroups) {
        QList<qsizetype> members;
        for (qsizetype i = 0; i < files.size(); ++i) {
            if (std::any_of(patterns.cbegin(), patterns.cend(), [&](const QString &p) {
                    return matchesGroupPattern(files.at(i).templatePath, p, cs);
                }))
                members.append(i);
        }
        if (members.size() > 1)
            groups.append(members);
    }
    return groups;
}

} // namespace

Result<vault::Snapshot> BackupService::backup(const GameTarget &target, const BackupOptions &options)
{
    if (target.profile.backup.requireGameClosed && isGameRunning(target, m_env.processes()))
        return Error{ErrorCode::PermissionDenied, QStringLiteral("close the game before backing it up"),
                     target.profile.id};

    const QList<profiles::ResolvedFile> files = profiles::PathResolver().resolve(target.profile, target.context);

    QList<vault::RegistryKey> registry;
    if (registryRulesApply(target, m_env)) {
        for (const profiles::RegistryRule &rule : target.profile.registry) {
            for (const QString &key : rule.keys) {
                auto tree = m_env.registry->readTree(key);
                if (!tree.ok())
                    return tree.error();
                for (const platform::RegistryKeyData &data : tree.value()) {
                    vault::RegistryKey captured;
                    captured.key = data.key;
                    captured.ruleId = rule.id;
                    for (const platform::RegistryValueData &value : data.values)
                        captured.values.append({value.name, value.type, value.data});
                    registry.append(captured);
                }
            }
        }
    }
    if (files.isEmpty() && registry.isEmpty())
        return Error{ErrorCode::NotFound, QStringLiteral("no save files found for this game"), target.profile.id};

    if (target.profile.backup.maxBytes && !options.allowLargerThanExpected) {
        qint64 bytes = 0;
        for (const profiles::ResolvedFile &file : files)
            bytes += QFileInfo(file.absolutePath).size();
        if (bytes > *target.profile.backup.maxBytes)
            return Error{ErrorCode::TooLarge, QStringLiteral("the saves are larger than this game's profile expects"),
                         QString::number(bytes)};
    }

    vault::SnapshotRequest request;
    request.gameId = target.effectiveGameId();
    request.kind = options.kind;
    request.trigger = options.trigger;
    request.label = options.label;
    request.slot = options.slot;
    request.machineId = m_env.machineId;
    request.appVersion = m_env.appVersion;
    request.profile = {target.channel, target.profile.id, target.profile.revision};
    for (const profiles::ResolvedFile &file : files)
        request.files.append({file.absolutePath, file.templatePath, file.ruleId, file.bindings});
    request.registry = registry;
    request.consistentGroups = consistentGroups(target, files);

    auto snapshot = m_vault.createSnapshot(request, options.onFileRead);
    if (snapshot.ok())
        qCInfo(lcBackup) << "backed up" << request.gameId << "-" << snapshot.value().files.size() << "file(s)";
    return snapshot;
}

} // namespace savevault::services
