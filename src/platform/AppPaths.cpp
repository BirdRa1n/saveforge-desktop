#include "savevault/platform/AppPaths.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace savevault::platform {

namespace {

// Moves the directory used before the project was renamed (".../SaveForge")
// to its new name, once. When the move fails (for example, an old version
// still running holds a file open), the old directory keeps being used so
// no settings or backups seem to disappear.
QString migratedDirectory(const QString &base)
{
    const QString current = base + QStringLiteral("/SaveVault");
    const QString legacy = base + QStringLiteral("/SaveForge");
    if (QFileInfo::exists(current) || !QFileInfo(legacy).isDir())
        return current;
    return QDir().rename(legacy, current) ? current : legacy;
}

} // namespace

QString appDataDirectory()
{
    const QString overridden = qEnvironmentVariable("SAVEVAULT_DATA_DIR");
    if (!overridden.isEmpty())
        return QDir::cleanPath(QDir::fromNativeSeparators(overridden));
    // GenericDataLocation does not depend on QCoreApplication::applicationName,
    // so the GUI ("SaveVault") and the CLI ("savevault-cli") share one directory.
    static const QString directory =
        migratedDirectory(QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)));
    return directory;
}

} // namespace savevault::platform
