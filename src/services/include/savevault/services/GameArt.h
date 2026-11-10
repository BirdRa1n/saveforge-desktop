#pragma once

#include "savevault/core/Result.h"
#include "savevault/services/Library.h"

#include <QString>
#include <QStringList>

namespace savevault::registry {
class Fetcher;
}

namespace savevault::services {

// Images that represent a game, as absolute file paths (empty when unknown).
struct GameArt {
    QString header; // landscape cover, about 460x215 (library cards, game page)
    QString hero;   // wide banner behind the game page title
    QString logo;   // transparent title logo drawn over the banner
    QString icon;   // small square icon, used when there is no cover

    [[nodiscard]] bool isEmpty() const { return header.isEmpty() && hero.isEmpty() && logo.isEmpty() && icon.isEmpty(); }
};

// Steam app ids that identify a game: the installation's first, then the profile's.
[[nodiscard]] QStringList steamAppIds(const LibraryGame &game);

// Finds the art launchers already keep on this machine, so covers work offline
// and without any request:
//   - Steam: <root>/appcache/librarycache/<appid>/{library_header,header,library_hero,logo}.*
//     and the older flat layout <appid>_header.jpg, ...;
//   - GOG: goggame-<id>.ico in the installation directory.
class LocalArtFinder {
public:
    explicit LocalArtFinder(QStringList steamRoots);

    [[nodiscard]] GameArt find(const LibraryGame &game) const;

private:
    QStringList m_steamCaches;
};

// Landscape covers downloaded from Steam's public CDN for games whose art is
// not on this machine (for example games that are not installed), kept in
// <dir>/steam/<appid>.jpg. A request names only the app id.
class SteamArtCache {
public:
    // Base URL of the Steam asset CDN; files are <base><appid>/header.jpg.
    static constexpr const char *kCdnBase = "https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/";

    explicit SteamArtCache(const QString &directory);

    // The cached cover of `appId`, or an empty string.
    [[nodiscard]] QString cachedHeader(const QString &appId) const;
    // True when an earlier download found no cover for `appId` (not retried).
    [[nodiscard]] bool knownMissing(const QString &appId) const;

    // Downloads and stores the cover of `appId`. Accepts only JPEG or PNG data.
    // NotFound when Steam has no cover for the id (remembered by knownMissing()).
    [[nodiscard]] Result<QString> download(const QString &appId, registry::Fetcher &fetcher);

private:
    QString m_dir;
};

} // namespace savevault::services
