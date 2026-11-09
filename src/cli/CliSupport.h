#pragma once

#include "saveforge/registry/Channel.h"
#include "saveforge/registry/ProfileCatalog.h"

#include <QString>
#include <QStringList>
#include <QTextStream>

namespace saveforge::cli {

// Version string of this build (e.g. "0.1.0").
[[nodiscard]] QString appVersion();

// <app data>/registry
[[nodiscard]] QString registryCacheDirectory();

struct CatalogLoad {
    registry::ProfileCatalog catalog;
    QList<registry::Notice> notices;
    QStringList warnings;
};

// Builds the profile catalog from every channel in the local registry cache.
// Problems (untrusted or damaged cache, expired index, test trust anchor) are
// returned as warnings; an empty catalog is not an error.
[[nodiscard]] CatalogLoad loadCatalog();

// Prints warnings to `err`, one per line.
void printWarnings(const QStringList &warnings, QTextStream &err);

} // namespace saveforge::cli
