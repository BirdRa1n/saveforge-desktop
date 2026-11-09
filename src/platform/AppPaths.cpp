#include "saveforge/platform/AppPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace saveforge::platform {

QString appDataDirectory()
{
    const QString overridden = qEnvironmentVariable("SAVEFORGE_DATA_DIR");
    if (!overridden.isEmpty())
        return QDir::cleanPath(QDir::fromNativeSeparators(overridden));
    // GenericDataLocation does not depend on QCoreApplication::applicationName,
    // so the GUI ("SaveForge") and the CLI ("saveforge-cli") share one directory.
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)) + QStringLiteral("/SaveForge");
}

} // namespace saveforge::platform
