#pragma once

#include "savevault/detection/DetectionEnvironment.h"
#include "savevault/detection/GameMatcher.h"
#include "savevault/detection/InstallCandidate.h"
#include "savevault/registry/Channel.h"
#include "savevault/registry/ProfileCatalog.h"
#include "savevault/services/GameTarget.h"
#include "savevault/vault/Vault.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace savevault::services {

// Profiles available on this machine, built from the local registry cache.
struct CatalogLoad {
    registry::ProfileCatalog catalog;
    QList<registry::Notice> notices;
    QStringList warnings;            // untrusted/damaged cache, expired channel, test trust anchor
    bool usingTestKeys = false;
};

// Loads every cached channel (re-verified) into one catalog. Channels the
// cache does not have yet are first seeded from the distribution bundled with
// the build, if any, so profiles work on first run without a download.
// Profiles in `localProfilesDir` (see LocalProfileStore) form the "local"
// channel, which takes precedence over the registry.
// Problems become warnings: an empty catalog is not an error.
[[nodiscard]] CatalogLoad loadCatalogFromCache(const QString &registryCacheDir, const QString &appVersion,
                                               const QString &localProfilesDir = {});

// One game shown in the library.
struct LibraryGame {
    GameTarget target;                                   // profile, channel and where it resolves
    detection::MatchReason reason = detection::MatchReason::SavesFound;
    std::optional<detection::InstallCandidate> install;  // empty when found only by its saves or backups
    qsizetype saveFilesFound = 0;
    std::optional<vault::Snapshot> lastSnapshot;
    qsizetype snapshotCount = 0;
    bool launcherCloud = false;                          // the installing store also syncs these saves

    [[nodiscard]] QString id() const { return target.effectiveGameId(); }
};

struct LibraryScan {
    QList<LibraryGame> games;                            // sorted by name
    QList<detection::InstallCandidate> unmatched;        // installed games without a profile
};

// Combines detected games with the vault: every detected game, plus games that
// are no longer detected but still have snapshots (so their backups stay
// reachable). `vault` may be null when no vault is configured.
[[nodiscard]] LibraryScan scanLibrary(const registry::ProfileCatalog &catalog,
                                      const QList<detection::InstallCandidate> &installs,
                                      const detection::DetectionEnvironment &env, const vault::Vault *vault);

} // namespace savevault::services
