#pragma once

#include "saveforge/core/Error.h"
#include "saveforge/registry/Metadata.h"
#include "saveforge/registry/RegistryCache.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace saveforge::registry {

// Channels a client follows unless the user changes it (registry spec
// section 7). `community-experimental` is opt-in.
[[nodiscard]] QStringList defaultChannels();

// Mirrors compiled into this build (CMake option SAVEFORGE_REGISTRY_MIRRORS),
// in the order they are tried. Empty when the build has none.
[[nodiscard]] QStringList builtinMirrors();

// Directory of the registry distribution compiled into this build (CMake
// option SAVEFORGE_REGISTRY_BUNDLED_DIR), as a Qt resource path, or an empty
// string. It seeds the cache on first run so profiles work before any download.
[[nodiscard]] QString bundledDistribution();

// Outcome for one channel.
struct ChannelUpdate {
    enum class Status {
        Updated,  // a newer sequence was verified and cached
        UpToDate, // the cache already has the mirror's sequence
        Failed,   // nothing changed; see `error`
    };

    QString channel;
    Status status = Status::Failed;
    qint64 sequence = 0;          // cached sequence after the update
    qsizetype profileCount = 0;   // valid profiles in that sequence (Updated only)
    bool expired = false;         // the index is past its expiry (Updated only)
    QString mirror;               // where the accepted data came from
    std::optional<Error> error;   // Failed only: the error of the last mirror tried
};

struct UpdateOptions {
    QStringList channels = defaultChannels();
    QString appVersion;
    QString userAgent;
};

// Downloads the channels from the first mirror that serves a version that
// verifies, and stores them in `cache` (registry spec section 8): follows the
// root chain, checks the index signature and sequence, skips the bundle when
// the sequence is already cached, and otherwise verifies and caches it. Any
// failure leaves the cached copy of that channel untouched.
[[nodiscard]] QList<ChannelUpdate> updateFromMirrors(const QStringList &mirrors, RegistryCache &cache,
                                                     const RootMetadata &anchor, const UpdateOptions &options);

// Verifies the channels of a distribution directory (a folder chosen by the
// user, or bundledDistribution()) and stores them in `cache`. With
// `onlyMissing`, channels the cache already has are left alone, so a bundled
// distribution never replaces a newer download. An empty `channels` means
// every channel the distribution contains.
[[nodiscard]] QList<ChannelUpdate> importDistribution(const QString &distributionDir, RegistryCache &cache,
                                                      const RootMetadata &anchor, const QString &appVersion,
                                                      const QStringList &channels = {}, bool onlyMissing = false);

} // namespace saveforge::registry
