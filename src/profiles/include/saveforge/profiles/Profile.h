#pragma once

#include "saveforge/platform/Os.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace saveforge::profiles {

// In-memory form of a game profile (saveforge-specs/profile/SPEC.md, v1).
// Instances produced by parseProfile() satisfy the schema and the path-safety
// rules; code that builds profiles by hand must validate them the same way.

enum class Tag { Save, Config };

struct Condition {
    std::optional<platform::Os> os;   // platform of the game build
    QString store;                    // empty: any store
};

struct SaveRule {
    QString id;
    QList<Tag> tags;
    Condition when;
    QStringList paths;                // path templates
};

struct RegistryRule {
    QString id;
    QList<Tag> tags;
    QStringList keys;                 // "HKCU/Software/..."
};

struct Identifiers {
    QList<qint64> steam;
    QList<qint64> gog;
    QStringList epic;
    QStringList executables;
    QStringList installDirNames;
};

struct BackupOptions {
    bool requireGameClosed = false;
    QStringList processNames;
    QStringList cloudSync;            // stores whose cloud covers the game
    std::optional<qint64> maxBytes;
    QList<QStringList> consistentGroups;
};

struct Mechanism {
    QString type;                     // "standard", "latest-n", "store-user-scoped"
    QJsonObject params;
};

struct Provenance {
    QString source;                   // "saveforge", "ludusavi", "local"
    QString license;                  // SPDX identifier
    QString basedOnLudusavi;
    QStringList contributors;
};

struct Profile {
    int schemaVersion = 1;
    QString id;
    QString name;
    int revision = 1;
    Identifiers identifiers;
    QList<SaveRule> saves;
    QList<RegistryRule> registry;
    QStringList exclude;
    BackupOptions backup;
    Mechanism mechanism;
    QString minAppVersion;
    QString gameVersions;
    Provenance provenance;
};

} // namespace saveforge::profiles
