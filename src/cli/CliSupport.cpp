#include "CliSupport.h"

#include "saveforge/core/Version.h"
#include "saveforge/platform/AppPaths.h"

namespace saveforge::cli {

QString appVersion()
{
    return QString::fromUtf8(kVersion.data(), static_cast<qsizetype>(kVersion.size()));
}

QString registryCacheDirectory()
{
    return platform::appDataDirectory() + QStringLiteral("/registry");
}

CatalogLoad loadCatalog()
{
    return services::loadCatalogFromCache(registryCacheDirectory(), appVersion());
}

void printWarnings(const QStringList &warnings, QTextStream &err)
{
    for (const QString &warning : warnings)
        err << "warning: " << warning << Qt::endl;
}

} // namespace saveforge::cli
