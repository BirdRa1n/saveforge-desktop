#include "saveforge/platform/UserFolders.h"

#include <QDir>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
#  include <qt_windows.h>
#  include <knownfolders.h>
#  include <shlobj.h>
#endif

namespace saveforge::platform {

namespace {

#if defined(Q_OS_WIN)
QString knownFolder(REFKNOWNFOLDERID id)
{
    PWSTR raw = nullptr;
    QString result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, nullptr, &raw)) && raw)
        result = QString::fromWCharArray(raw);
    CoTaskMemFree(raw);
    return result;
}
#endif

#if defined(Q_OS_LINUX) || (!defined(Q_OS_WIN) && !defined(Q_OS_MACOS))
// XDG base directory variables only count when they hold absolute paths.
QString xdgDir(const char *variable, const QString &fallback)
{
    const QString value = qEnvironmentVariable(variable);
    return QDir::isAbsolutePath(value) ? value : fallback;
}
#endif

} // namespace

FolderMap hostUserFolders()
{
    FolderMap folders;
    const QString home = QDir::homePath();
    folders.set(QStringLiteral("home"), home);

#if defined(Q_OS_WIN)
    folders.set(QStringLiteral("home"), knownFolder(FOLDERID_Profile));
    folders.set(QStringLiteral("winAppData"), knownFolder(FOLDERID_RoamingAppData));
    folders.set(QStringLiteral("winLocalAppData"), knownFolder(FOLDERID_LocalAppData));
    folders.set(QStringLiteral("winLocalAppDataLow"), knownFolder(FOLDERID_LocalAppDataLow));
    folders.set(QStringLiteral("winDocuments"), knownFolder(FOLDERID_Documents));
    folders.set(QStringLiteral("winSavedGames"), knownFolder(FOLDERID_SavedGames));
    folders.set(QStringLiteral("winPublic"), knownFolder(FOLDERID_Public));
    folders.set(QStringLiteral("winProgramData"), knownFolder(FOLDERID_ProgramData));
#elif defined(Q_OS_MACOS)
    folders.set(QStringLiteral("macAppSupport"), home + QStringLiteral("/Library/Application Support"));
    folders.set(QStringLiteral("macPreferences"), home + QStringLiteral("/Library/Preferences"));
    folders.set(QStringLiteral("macContainers"), home + QStringLiteral("/Library/Containers"));
#else
    folders.set(QStringLiteral("xdgData"), xdgDir("XDG_DATA_HOME", home + QStringLiteral("/.local/share")));
    folders.set(QStringLiteral("xdgConfig"), xdgDir("XDG_CONFIG_HOME", home + QStringLiteral("/.config")));
#endif
    return folders;
}

} // namespace saveforge::platform
