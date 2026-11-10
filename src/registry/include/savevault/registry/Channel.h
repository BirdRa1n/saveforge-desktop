#pragma once

#include "savevault/core/Result.h"
#include "savevault/profiles/Profile.h"
#include "savevault/registry/Metadata.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace savevault::registry {

struct Notice {
    QString license;
    QString attribution;
    QString url;
};

// A channel whose metadata and bundle passed every check of the registry spec.
struct VerifiedChannel {
    QString channel;
    RootMetadata root;                 // trusted root after following the rotation chain
    ChannelIndex index;
    QList<profiles::Profile> profiles; // valid, supported and not revoked
    QList<Notice> notices;             // license notices clients must display
    QStringList rejectedProfiles;      // "<id>: <reason>" for profiles that were dropped
    bool expired = false;              // index past its expiry: usable, but warn
};

struct LoadOptions {
    qint64 minimumSequence = 0;        // last sequence accepted for this channel (anti-rollback)
    QString appVersion;                // compared with the index's minAppVersion
    QDateTime now = QDateTime::currentDateTimeUtc();
};

// Follows roots/<N+1>.json from `trusted` while each new version is signed by
// a threshold of the previous root keys (registry spec section 3).
[[nodiscard]] Result<RootMetadata> updateRootChain(const QString &distributionDir, const RootMetadata &trusted);

// Loads one channel from a distribution directory laid out as in registry spec
// section 1 (a download or the local cache) and verifies it (sections 4 and 5):
// root chain, index signature by an online key allowed for the channel,
// anti-rollback, minimum app version, bundle size and SHA-256, bounded zstd
// decompression, bundle header and every profile. Errors: Untrusted for
// signature, chain or rollback failures; Unsupported when the app is too old;
// Corrupted or NotFound for damaged or missing files.
[[nodiscard]] Result<VerifiedChannel> loadChannel(const QString &distributionDir, const QString &channel,
                                                  const RootMetadata &trustAnchor, const LoadOptions &options);

} // namespace savevault::registry
