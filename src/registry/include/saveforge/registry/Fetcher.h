#pragma once

#include "saveforge/core/Result.h"

#include <QByteArray>
#include <QString>

#include <memory>

namespace saveforge::registry {

// Reads files of a registry distribution from one mirror (registry spec
// section 1). Paths are relative to the mirror's base URL, e.g.
// "channels/official/index.json". Nothing returned by a fetcher is trusted:
// callers verify it like any other distribution.
//
// Errors: NotFound when the mirror does not have the file (HTTP 404/410),
// Corrupted when it is larger than `maxBytes`, Network or Io otherwise.
class Fetcher {
public:
    virtual ~Fetcher() = default;

    [[nodiscard]] virtual Result<QByteArray> fetch(const QString &relativePath, qint64 maxBytes) = 0;
    // The mirror as shown to users and in logs.
    [[nodiscard]] virtual QString location() const = 0;
};

// Largest file a registry client downloads. Bundles of the whole Ludusavi
// manifest are about 3 MB compressed; the limit leaves ample room.
inline constexpr qint64 kMaxRegistryFileBytes = 64LL * 1024 * 1024;

// Fetcher for a mirror given as configured by the user or the build:
//   https://host/path/v1/          downloaded over HTTPS
//   http://localhost:8080/v1/      plain HTTP, accepted only for loopback hosts
//   file:///D:/registry/v1, D:/registry/v1   a local directory
// Returns InvalidArgument for anything else. HTTP fetchers block the calling
// thread (run them on a worker) and identify themselves only with
// `userAgent`; requests carry no other information about the user.
[[nodiscard]] Result<std::unique_ptr<Fetcher>> makeFetcher(const QString &mirror, const QString &userAgent);

} // namespace saveforge::registry
