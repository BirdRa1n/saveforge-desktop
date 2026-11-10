#include "savevault/detection/InstallCandidate.h"

#include "savevault/platform/UserFolders.h"

#include <QDir>
#include <QFileInfo>

namespace savevault::detection {

namespace {

constexpr qsizetype kMaxExecutables = 32;

bool isExecutableFor(const QFileInfo &info, platform::Os os)
{
    switch (os) {
    case platform::Os::Windows:
        return info.isFile() && info.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0;
    case platform::Os::MacOS:
        return info.isDir() && info.suffix().compare(QLatin1String("app"), Qt::CaseInsensitive) == 0;
    case platform::Os::Linux:
        return info.isFile() && info.isExecutable() && info.suffix() != u"so" && !info.fileName().contains(u".so.");
    }
    return false;
}

void collectExecutables(const QString &dir, platform::Os os, int depth, QStringList &out)
{
    const QFileInfoList entries = QDir(dir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (out.size() >= kMaxExecutables)
            return;
        if (entry.isSymLink() || entry.isJunction())
            continue;
        if (isExecutableFor(entry, os)) {
            if (!out.contains(entry.fileName(), Qt::CaseInsensitive))
                out.append(entry.fileName());
        } else if (entry.isDir() && depth > 0) {
            collectExecutables(entry.absoluteFilePath(), os, depth - 1, out);
        }
    }
}

} // namespace

QStringList findExecutables(const QString &dir, platform::Os os)
{
    QStringList executables;
    if (!dir.isEmpty() && QFileInfo(dir).isDir())
        collectExecutables(dir, os, 1, executables);
    return executables;
}

QString normalizeWinePrefix(const QString &dir)
{
    if (dir.isEmpty())
        return {};
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(dir));
    if (QFileInfo(clean + QStringLiteral("/drive_c")).isDir())
        return clean;
    if (QFileInfo(clean + QStringLiteral("/pfx/drive_c")).isDir())
        return clean + QStringLiteral("/pfx");
    return {};
}

QString winePrefixUser(const QString &prefixDir)
{
    const QStringList users = QDir(prefixDir + QStringLiteral("/drive_c/users"))
                                  .entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (users.contains(QStringLiteral("steamuser")))
        return QStringLiteral("steamuser");
    for (const QString &user : users) {
        if (user.compare(QLatin1String("Public"), Qt::CaseInsensitive) != 0)
            return user;
    }
    return {};
}

profiles::ResolveContext resolveContextFor(const InstallCandidate &candidate, const DetectionEnvironment &env)
{
    profiles::ResolveContext context;
    context.gameOs = candidate.gameOs;
    context.store = candidate.store;
    context.storeUserIds = candidate.storeUserIds;
    context.storeGameId = candidate.storeId;
    context.osUserName = env.osUserName;
    if (!candidate.winePrefix.isEmpty()) {
        context.folders = platform::winePrefixFolders(candidate.winePrefix, candidate.wineUser);
        context.caseInsensitive = true;
    } else {
        context.folders = env.userFolders;
        context.caseInsensitive = env.hostOs != platform::Os::Linux;
    }
    context.folders.set(QStringLiteral("base"), candidate.installDir);
    return context;
}

profiles::ResolveContext hostResolveContext(const DetectionEnvironment &env, const QStringList &storeUserIds)
{
    profiles::ResolveContext context;
    context.gameOs = env.hostOs;
    context.folders = env.userFolders;
    context.caseInsensitive = env.hostOs != platform::Os::Linux;
    context.storeUserIds = storeUserIds;
    context.osUserName = env.osUserName;
    return context;
}

} // namespace savevault::detection
