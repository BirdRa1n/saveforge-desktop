#pragma once

#include "saveforge/services/Library.h"

#include <QString>
#include <QStringList>
#include <QTextStream>

namespace saveforge::cli {

// Version string of this build (e.g. "0.1.0").
[[nodiscard]] QString appVersion();

// <app data>/registry
[[nodiscard]] QString registryCacheDirectory();

// <app data>/profiles: local profiles and manual games.
[[nodiscard]] QString localProfilesDirectory();

using CatalogLoad = services::CatalogLoad;

// The profile catalog: local profiles plus the registry cache (see services::loadCatalogFromCache).
[[nodiscard]] CatalogLoad loadCatalog();

// Prints warnings to `err`, one per line.
void printWarnings(const QStringList &warnings, QTextStream &err);

} // namespace saveforge::cli
