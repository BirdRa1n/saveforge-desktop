#pragma once

#include "savevault/core/Result.h"
#include "savevault/registry/Channel.h"
#include "savevault/registry/Metadata.h"

#include <QString>
#include <QStringList>

namespace savevault::registry {

// Root metadata compiled into this build (CMake option
// SAVEVAULT_REGISTRY_TRUST_ANCHOR). Every registry download is verified
// against it.
[[nodiscard]] Result<RootMetadata> builtinTrustAnchor();

// True when the build trusts the PUBLIC test keys of savevault-specs: fine for
// development, never for releases. User interfaces should say so.
[[nodiscard]] bool builtinTrustAnchorIsForTesting();

// Verified copies of registry channels, kept on disk so profiles stay available
// offline (registry spec sections 4 and 5):
//
//   <dir>/state.json                       last accepted sequence per channel
//   <dir>/<channel>/<sequence>/...         a verified distribution subset
//
// Only distributions that pass loadChannel() are stored, at most three per
// channel. Loading re-verifies the cached files and falls back to an older
// sequence if the newest is damaged.
class RegistryCache {
public:
    explicit RegistryCache(const QString &directory);

    // Verifies `channel` in `distributionDir` (anti-rollback against the last
    // accepted sequence) and stores it. Importing the same sequence again is a no-op.
    [[nodiscard]] Result<VerifiedChannel> importChannel(const QString &distributionDir, const QString &channel,
                                                        const RootMetadata &anchor, const QString &appVersion);

    // Newest cached copy of `channel` that still verifies.
    [[nodiscard]] Result<VerifiedChannel> loadChannel(const QString &channel, const RootMetadata &anchor,
                                                      const QString &appVersion) const;

    [[nodiscard]] QStringList channels() const;
    [[nodiscard]] qint64 lastSequence(const QString &channel) const;
    [[nodiscard]] const QString &directory() const { return m_dir; }

    static constexpr int kKeptVersions = 3;

private:
    [[nodiscard]] QList<qint64> cachedSequences(const QString &channel) const;
    [[nodiscard]] Result<void> setLastSequence(const QString &channel, qint64 sequence);

    QString m_dir;
};

} // namespace savevault::registry
