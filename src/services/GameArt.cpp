#include "saveforge/services/GameArt.h"

#include "saveforge/core/Uuid.h"
#include "saveforge/registry/Fetcher.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace saveforge::services {

namespace {

constexpr qint64 kMaxCoverBytes = 4 * 1024 * 1024;

QString firstExisting(const QStringList &paths)
{
    for (const QString &path : paths) {
        if (QFile::exists(path))
            return path;
    }
    return {};
}

bool isAppId(const QString &value)
{
    static const QRegularExpression digits(QStringLiteral("^[0-9]{1,10}$"));
    return digits.match(value).hasMatch();
}

bool looksLikeImage(const QByteArray &bytes)
{
    return bytes.startsWith("\xFF\xD8\xFF") || bytes.startsWith("\x89PNG\r\n\x1A\n");
}

} // namespace

QStringList steamAppIds(const LibraryGame &game)
{
    QStringList ids;
    if (game.install && game.install->store == u"steam" && isAppId(game.install->storeId))
        ids.append(game.install->storeId);
    for (const qint64 id : game.target.profile.identifiers.steam) {
        const QString value = QString::number(id);
        if (!ids.contains(value))
            ids.append(value);
    }
    return ids;
}

LocalArtFinder::LocalArtFinder(QStringList steamRoots)
{
    for (const QString &root : steamRoots) {
        const QString cache = QDir::cleanPath(root) + QStringLiteral("/appcache/librarycache");
        if (QDir(cache).exists())
            m_steamCaches.append(cache);
    }
}

GameArt LocalArtFinder::find(const LibraryGame &game) const
{
    GameArt art;
    for (const QString &appId : steamAppIds(game)) {
        for (const QString &cache : m_steamCaches) {
            const QString dir = cache + u'/' + appId + u'/';
            const QString flat = cache + u'/' + appId + u'_';
            if (art.header.isEmpty())
                art.header = firstExisting({dir + QStringLiteral("library_header.jpg"), dir + QStringLiteral("header.jpg"),
                                            flat + QStringLiteral("header.jpg")});
            if (art.hero.isEmpty())
                art.hero = firstExisting({dir + QStringLiteral("library_hero.jpg"), flat + QStringLiteral("library_hero.jpg")});
            if (art.logo.isEmpty())
                art.logo = firstExisting({dir + QStringLiteral("logo.png"), flat + QStringLiteral("logo.png")});
        }
        if (!art.header.isEmpty())
            break;
    }
    if (game.install && game.install->store == u"gog" && !game.install->installDir.isEmpty()) {
        art.icon = firstExisting({game.install->installDir + QStringLiteral("/goggame-") + game.install->storeId
                                  + QStringLiteral(".ico")});
    }
    return art;
}

SteamArtCache::SteamArtCache(const QString &directory)
    : m_dir(QDir::cleanPath(directory) + QStringLiteral("/steam"))
{
}

QString SteamArtCache::cachedHeader(const QString &appId) const
{
    if (!isAppId(appId))
        return {};
    return firstExisting({m_dir + u'/' + appId + QStringLiteral(".jpg")});
}

bool SteamArtCache::knownMissing(const QString &appId) const
{
    return isAppId(appId) && QFile::exists(m_dir + u'/' + appId + QStringLiteral(".missing"));
}

Result<QString> SteamArtCache::download(const QString &appId, registry::Fetcher &fetcher)
{
    if (!isAppId(appId))
        return Error{ErrorCode::InvalidArgument, QStringLiteral("not a Steam app id"), appId};
    if (!QDir().mkpath(m_dir))
        return Error{ErrorCode::Io, QStringLiteral("cannot create the cover cache"), m_dir};

    auto bytes = fetcher.fetch(appId + QStringLiteral("/header.jpg"), kMaxCoverBytes);
    if (!bytes.ok()) {
        if (bytes.error().code == ErrorCode::NotFound) {
            QFile marker(m_dir + u'/' + appId + QStringLiteral(".missing"));
            if (marker.open(QIODevice::WriteOnly))
                marker.close();
        }
        return bytes.error();
    }
    if (!looksLikeImage(bytes.value()))
        return Error{ErrorCode::Corrupted, QStringLiteral("the cover is not an image"), appId};

    const QString target = m_dir + u'/' + appId + QStringLiteral(".jpg");
    const QString tmp = target + u'.' + uuidToString(createUuidV7()).right(12) + QStringLiteral(".tmp");
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly) || out.write(bytes.value()) != bytes.value().size()) {
        out.close();
        QFile::remove(tmp);
        return Error{ErrorCode::Io, QStringLiteral("cannot store the cover"), tmp};
    }
    out.close();
    QFile::remove(target);
    if (!QFile::rename(tmp, target)) {
        QFile::remove(tmp);
        return Error{ErrorCode::Io, QStringLiteral("cannot store the cover"), target};
    }
    return target;
}

} // namespace saveforge::services
