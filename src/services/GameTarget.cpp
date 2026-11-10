#include "saveforge/services/GameTarget.h"

#include "saveforge/profiles/PathResolver.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace saveforge::services {

bool isGameRunning(const GameTarget &target, const QList<platform::ProcessInfo> &processes)
{
    const QStringList &names = target.profile.backup.processNames;
    QString installDir = target.context.folders.value(QStringLiteral("base"));
    if (!installDir.isEmpty())
        installDir = QDir::cleanPath(installDir) + u'/';

    for (const platform::ProcessInfo &process : processes) {
        if (names.contains(process.name, Qt::CaseInsensitive))
            return true;
        if (!installDir.isEmpty() && !process.executablePath.isEmpty()
            && QDir::cleanPath(process.executablePath).startsWith(installDir, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool registryRulesApply(const GameTarget &target, const ServiceEnvironment &env)
{
    return !target.profile.registry.isEmpty() && env.hostOs == platform::Os::Windows
           && target.context.gameOs == platform::Os::Windows && env.registry && env.registry->available();
}

QStringList saveFolders(const GameTarget &target)
{
    const Qt::CaseSensitivity cs = target.context.caseInsensitive ? Qt::CaseInsensitive : Qt::CaseSensitive;
    const auto isInside = [cs](const QString &path, const QString &dir) {
        return path.compare(dir, cs) == 0 || path.startsWith(dir + u'/', cs);
    };

    const profiles::PathResolver resolver;
    QStringList candidates;
    for (const profiles::SaveRule &rule : target.profile.saves) {
        for (const QString &path : rule.paths) {
            if (!resolver.ruleApplies(rule, path, target.context))
                continue;
            const QStringList segments = path.split(u'/');
            const QString root = segments.first();
            QString dir = target.context.folders.value(root.mid(1, root.size() - 2));
            if (dir.isEmpty())
                continue;
            dir = QDir::cleanPath(dir);
            for (qsizetype i = 1; i < segments.size(); ++i) {
                const QString &segment = segments.at(i);
                if (segment.contains(u'*') || segment.contains(u'?') || segment.contains(u'<'))
                    break;
                // The last segment may name a file rather than a folder.
                if (i == segments.size() - 1 && !QFileInfo(dir + u'/' + segment).isDir())
                    break;
                dir += u'/' + segment;
            }
            if (QFileInfo(dir).isDir() && !candidates.contains(dir, cs))
                candidates.append(dir);
        }
    }

    // Prefer specific folders over the root folders they live in (a rule like
    // "<winDocuments>/*.sav" would otherwise offer all of Documents).
    const QStringList roots = [&] {
        QStringList dirs;
        for (const QString &name : target.context.folders.placeholders())
            dirs.append(QDir::cleanPath(target.context.folders.value(name)));
        return dirs;
    }();
    if (std::any_of(candidates.begin(), candidates.end(), [&](const QString &d) { return !roots.contains(d, cs); }))
        candidates.removeIf([&](const QString &d) { return roots.contains(d, cs); });

    // Keep outer folders only.
    QStringList folders;
    std::sort(candidates.begin(), candidates.end(), [](const QString &a, const QString &b) { return a.size() < b.size(); });
    for (const QString &dir : std::as_const(candidates)) {
        if (std::none_of(folders.begin(), folders.end(), [&](const QString &outer) { return isInside(dir, outer); }))
            folders.append(dir);
    }

    // Folders with saves first.
    const QList<profiles::ResolvedFile> files = resolver.resolve(target.profile, target.context);
    std::stable_sort(folders.begin(), folders.end(), [&](const QString &a, const QString &b) {
        const auto holdsSaves = [&](const QString &dir) {
            return std::any_of(files.begin(), files.end(), [&](const profiles::ResolvedFile &f) { return isInside(f.absolutePath, dir); });
        };
        return holdsSaves(a) && !holdsSaves(b);
    });
    return folders;
}

} // namespace saveforge::services
