#include "CliSupport.h"

#include "savevault/core/Version.h"
#include "savevault/platform/AppPaths.h"

namespace savevault::cli {

QString appVersion()
{
    return QString::fromUtf8(kVersion.data(), static_cast<qsizetype>(kVersion.size()));
}

QString registryCacheDirectory()
{
    return platform::appDataDirectory() + QStringLiteral("/registry");
}

QString localProfilesDirectory()
{
    return platform::appDataDirectory() + QStringLiteral("/profiles");
}

QString exportLogDirectory()
{
    return platform::appDataDirectory() + QStringLiteral("/exports");
}

CatalogLoad loadCatalog()
{
    return services::loadCatalogFromCache(registryCacheDirectory(), appVersion(), localProfilesDirectory());
}

void printWarnings(const QStringList &warnings, QTextStream &err)
{
    for (const QString &warning : warnings)
        err << "warning: " << warning << Qt::endl;
}

} // namespace savevault::cli
