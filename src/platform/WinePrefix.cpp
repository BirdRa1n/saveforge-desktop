#include "saveforge/platform/UserFolders.h"

#include <QDir>
#include <QFileInfo>

namespace saveforge::platform {

namespace {

// Returns the first candidate that exists, or the first candidate if none do.
// Older Wine prefixes use localized/legacy names ("My Documents").
QString firstExisting(const QString &base, std::initializer_list<const char *> candidates)
{
    for (const char *name : candidates) {
        const QString path = base + QLatin1Char('/') + QLatin1String(name);
        if (QFileInfo(path).isDir())
            return path;
    }
    return base + QLatin1Char('/') + QLatin1String(*candidates.begin());
}

} // namespace

FolderMap winePrefixFolders(const QString &prefixDir, const QString &userName)
{
    FolderMap folders;
    if (prefixDir.isEmpty() || userName.isEmpty())
        return folders;

    const QString driveC = QDir::cleanPath(QDir::fromNativeSeparators(prefixDir)) + QStringLiteral("/drive_c");
    const QString profile = driveC + QStringLiteral("/users/") + userName;

    folders.set(QStringLiteral("home"), profile);
    folders.set(QStringLiteral("winAppData"), profile + QStringLiteral("/AppData/Roaming"));
    folders.set(QStringLiteral("winLocalAppData"), profile + QStringLiteral("/AppData/Local"));
    folders.set(QStringLiteral("winLocalAppDataLow"), profile + QStringLiteral("/AppData/LocalLow"));
    folders.set(QStringLiteral("winDocuments"), firstExisting(profile, {"Documents", "My Documents"}));
    folders.set(QStringLiteral("winSavedGames"), profile + QStringLiteral("/Saved Games"));
    folders.set(QStringLiteral("winPublic"), driveC + QStringLiteral("/users/Public"));
    folders.set(QStringLiteral("winProgramData"), driveC + QStringLiteral("/ProgramData"));
    return folders;
}

} // namespace saveforge::platform
