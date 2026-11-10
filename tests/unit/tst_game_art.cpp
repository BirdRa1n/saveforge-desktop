#include "ServiceFixture.h"

#include "savevault/registry/Fetcher.h"
#include "savevault/services/GameArt.h"

using namespace savevault;
using namespace savevault::services;
using namespace savevault::testing;

namespace {

const QByteArray kJpeg("\xFF\xD8\xFF\xE0 fake jpeg", 15);

LibraryGame steamGame(const QString &installedId, QList<qint64> profileIds = {})
{
    LibraryGame game;
    game.target.profile.id = QStringLiteral("game");
    game.target.profile.identifiers.steam = profileIds;
    if (!installedId.isEmpty()) {
        detection::InstallCandidate install;
        install.store = QStringLiteral("steam");
        install.storeId = installedId;
        game.install = install;
    }
    return game;
}

} // namespace

class TestGameArt : public QObject {
    Q_OBJECT

private slots:
    void appIdsPreferTheInstallation()
    {
        QCOMPARE(steamAppIds(steamGame(QStringLiteral("10"), {20, 10})), (QStringList{QStringLiteral("10"), QStringLiteral("20")}));
        QCOMPARE(steamAppIds(steamGame({}, {30})), QStringList{QStringLiteral("30")});
        QVERIFY(steamAppIds(steamGame(QStringLiteral("not-a-number"))).isEmpty());
    }

    void findsSteamLibraryCacheInBothLayouts()
    {
        QTemporaryDir steam;
        const QString cache = steam.path() + QStringLiteral("/appcache/librarycache");
        // Current layout: one folder per app.
        writeFile(cache + QStringLiteral("/10/header.jpg"), kJpeg);
        writeFile(cache + QStringLiteral("/10/library_header.jpg"), kJpeg);
        writeFile(cache + QStringLiteral("/10/library_hero.jpg"), kJpeg);
        writeFile(cache + QStringLiteral("/10/logo.png"), "png");
        // Older flat layout.
        writeFile(cache + QStringLiteral("/20_header.jpg"), kJpeg);

        const LocalArtFinder finder({steam.path(), steam.path() + QStringLiteral("/missing")});
        const GameArt current = finder.find(steamGame(QStringLiteral("10")));
        QCOMPARE(current.header, cache + QStringLiteral("/10/library_header.jpg"));
        QCOMPARE(current.hero, cache + QStringLiteral("/10/library_hero.jpg"));
        QCOMPARE(current.logo, cache + QStringLiteral("/10/logo.png"));

        // Not installed through Steam, but the profile knows its app id.
        const GameArt flat = finder.find(steamGame({}, {20}));
        QCOMPARE(flat.header, cache + QStringLiteral("/20_header.jpg"));
        QVERIFY(flat.hero.isEmpty());

        QVERIFY(finder.find(steamGame({}, {30})).isEmpty());
    }

    void findsTheGogIcon()
    {
        QTemporaryDir install;
        writeFile(install.path() + QStringLiteral("/goggame-1207658924.ico"), "ico");
        LibraryGame game;
        detection::InstallCandidate candidate;
        candidate.store = QStringLiteral("gog");
        candidate.storeId = QStringLiteral("1207658924");
        candidate.installDir = install.path();
        game.install = candidate;
        QCOMPARE(LocalArtFinder({}).find(game).icon, install.path() + QStringLiteral("/goggame-1207658924.ico"));
    }

    void downloadsAndCachesSteamCovers()
    {
        QTemporaryDir cdn;
        writeFile(cdn.path() + QStringLiteral("/10/header.jpg"), kJpeg);
        writeFile(cdn.path() + QStringLiteral("/20/header.jpg"), "<html>not an image</html>");
        auto fetcher = registry::makeFetcher(cdn.path(), {});
        QVERIFY(fetcher.ok());

        QTemporaryDir dir;
        SteamArtCache cache(dir.path());
        QVERIFY(cache.cachedHeader(QStringLiteral("10")).isEmpty());
        const auto downloaded = cache.download(QStringLiteral("10"), *fetcher.value());
        QVERIFY(downloaded.ok());
        QCOMPARE(cache.cachedHeader(QStringLiteral("10")), downloaded.value());

        QCOMPARE(cache.download(QStringLiteral("20"), *fetcher.value()).error().code, ErrorCode::Corrupted);
        QVERIFY(cache.cachedHeader(QStringLiteral("20")).isEmpty());

        QVERIFY(!cache.knownMissing(QStringLiteral("30")));
        QCOMPARE(cache.download(QStringLiteral("30"), *fetcher.value()).error().code, ErrorCode::NotFound);
        QVERIFY(cache.knownMissing(QStringLiteral("30")));

        QCOMPARE(cache.download(QStringLiteral("../x"), *fetcher.value()).error().code, ErrorCode::InvalidArgument);
    }
};

QTEST_GUILESS_MAIN(TestGameArt)
#include "tst_game_art.moc"
