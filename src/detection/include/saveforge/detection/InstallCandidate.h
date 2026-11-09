#pragma once

#include "saveforge/detection/DetectionEnvironment.h"
#include "saveforge/platform/Os.h"
#include "saveforge/profiles/PathResolver.h"

#include <QString>
#include <QStringList>

namespace saveforge::detection {

// An installed game reported by a launcher, before it is matched to a profile.
struct InstallCandidate {
    QString store;               // spec store name: "steam", "epic", "gog", ...
    QString launcher;            // client that installed it when it differs from the store, e.g. "heroic"
    QString storeId;             // Steam app id, Epic app name, GOG product id
    QString name;                // display name reported by the launcher
    QString installDir;          // absolute, '/'-separated
    QStringList executables;     // executable file names found in installDir
    platform::Os gameOs = platform::hostOs(); // platform of the game build
    QString winePrefix;          // set when a Windows build runs through Wine/Proton
    QString wineUser;            // Windows user inside the prefix ("steamuser" for Proton)
    QStringList storeUserIds;    // store accounts on this machine (values for <storeUserId>)
};

// Context for resolving a profile's paths for this installation: folders of the
// Wine/Proton prefix or of the host user, <base> = installDir, store ids, and
// case-insensitive matching unless a native Linux game is resolved on Linux.
[[nodiscard]] profiles::ResolveContext resolveContextFor(const InstallCandidate &candidate,
                                                         const DetectionEnvironment &env);

// Context for finding saves of games that are not installed or not detected
// ("save-first" scan): host folders, no <base>.
[[nodiscard]] profiles::ResolveContext hostResolveContext(const DetectionEnvironment &env,
                                                          const QStringList &storeUserIds);

// Executable names in `dir` and its direct subdirectories for a build of `os`:
// *.exe for Windows, *.app bundles for macOS, files with the execute bit for Linux.
[[nodiscard]] QStringList findExecutables(const QString &dir, platform::Os os);

// The Windows user directory inside a prefix ("steamuser" preferred), or empty.
[[nodiscard]] QString winePrefixUser(const QString &prefixDir);

// Accepts a prefix directory or its parent when the prefix lives in "pfx/"
// (Proton layout); returns the directory that contains drive_c, or empty.
[[nodiscard]] QString normalizeWinePrefix(const QString &dir);

} // namespace saveforge::detection
