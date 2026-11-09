#include "CliSupport.h"

#include "saveforge/core/Version.h"
#include "saveforge/platform/AppPaths.h"
#include "saveforge/registry/RegistryCache.h"

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
    CatalogLoad result;
    const auto anchor = registry::builtinTrustAnchor();
    if (!anchor.ok()) {
        result.warnings.append(QStringLiteral("registry disabled: ") + anchor.error().toString());
        return result;
    }
    if (registry::builtinTrustAnchorIsForTesting())
        result.warnings.append(QStringLiteral("this build trusts the PUBLIC registry test keys (development only)"));

    const registry::RegistryCache cache(registryCacheDirectory());
    for (const QString &channel : cache.channels()) {
        auto loaded = cache.loadChannel(channel, anchor.value(), appVersion());
        if (!loaded.ok()) {
            result.warnings.append(QStringLiteral("channel %1 unavailable: %2").arg(channel, loaded.error().toString()));
            continue;
        }
        if (loaded.value().expired)
            result.warnings.append(QStringLiteral("channel %1 is past its expiry date; update the registry").arg(channel));
        result.catalog.addChannel(channel, loaded.value().profiles);
        result.notices += loaded.value().notices;
    }
    return result;
}

void printWarnings(const QStringList &warnings, QTextStream &err)
{
    for (const QString &warning : warnings)
        err << "warning: " << warning << Qt::endl;
}

} // namespace saveforge::cli
