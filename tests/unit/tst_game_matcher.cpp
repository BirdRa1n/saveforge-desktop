#include "saveforge/detection/GameMatcher.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace saveforge;
using namespace saveforge::detection;
using saveforge::platform::Os;

namespace {

profiles::Profile profile(const QString &id, const profiles::Identifiers &identifiers,
                          const QStringList &paths = {QStringLiteral("<home>/Unused/save.dat")})
{
    profiles::Profile p;
    p.id = id;
    p.name = id;
    p.identifiers = identifiers;
    profiles::SaveRule rule;
    rule.id = QStringLiteral("progress");
    rule.tags = {profiles::Tag::Save};
    rule.paths = paths;
    p.saves = {rule};
    p.mechanism.type = QStringLiteral("standard");
    return p;
}

InstallCandidate install(const QString &store, const QString &storeId, const QString &dir,
                         const QStringList &executables = {}, const QString &name = {})
{
    InstallCandidate c;
    c.store = store;
    c.storeId = storeId;
    c.installDir = dir;
    c.executables = executables;
    c.name = name;
    return c;
}

QList<profiles::Profile> catalog()
{
    profiles::Identifiers hk;
    hk.steam = {367520};
    hk.gog = {1308320804};
    hk.executables = {QStringLiteral("hollow_knight.exe")};
    hk.installDirNames = {QStringLiteral("Hollow Knight")};

    profiles::Identifiers hades;
    hades.epic = {QStringLiteral("Min")};
    hades.executables = {QStringLiteral("Hades.exe"), QStringLiteral("game.exe")};

    profiles::Identifiers other;
    other.executables = {QStringLiteral("game.exe")}; // shared with hades: ambiguous
    other.installDirNames = {QStringLiteral("Other Game")};

    return {profile(QStringLiteral("hollow-knight"), hk), profile(QStringLiteral("hades"), hades),
            profile(QStringLiteral("other-game"), other)};
}

} // namespace

class TestGameMatcher : public QObject {
    Q_OBJECT

private slots:
    void matchesByStoreIdFirst()
    {
        const GameMatcher matcher(catalog());
        auto m = matcher.match(install(QStringLiteral("steam"), QStringLiteral("367520"), QStringLiteral("/x/whatever")));
        QVERIFY(m);
        QCOMPARE(m->profileId, QStringLiteral("hollow-knight"));
        QCOMPARE(m->reason, MatchReason::StoreId);

        m = matcher.match(install(QStringLiteral("epic"), QStringLiteral("Min"), QStringLiteral("/x/y")));
        QCOMPARE(m->profileId, QStringLiteral("hades"));
        // Store ids are store-specific: a GOG id is not a Steam id.
        QVERIFY(!matcher.match(install(QStringLiteral("steam"), QStringLiteral("1308320804"), QStringLiteral("/x/z"))));
    }

    void matchesByUniqueExecutable()
    {
        const GameMatcher matcher(catalog());
        const auto m = matcher.match(install(QStringLiteral("manual"), {}, QStringLiteral("/games/hk"),
                                             {QStringLiteral("Hollow_Knight.EXE"), QStringLiteral("UnityCrashHandler64.exe")}));
        QVERIFY(m);
        QCOMPARE(m->profileId, QStringLiteral("hollow-knight"));
        QCOMPARE(m->reason, MatchReason::Executable);
    }

    void ambiguousExecutablesDoNotMatch()
    {
        const GameMatcher matcher(catalog());
        QVERIFY(!matcher.match(install(QStringLiteral("manual"), {}, QStringLiteral("/games/x"), {QStringLiteral("game.exe")})));
    }

    void matchesByInstallFolderOrTitle()
    {
        const GameMatcher matcher(catalog());
        auto m = matcher.match(install(QStringLiteral("gog"), QStringLiteral("42"), QStringLiteral("/games/hollow_knight")));
        QVERIFY(m);
        QCOMPARE(m->profileId, QStringLiteral("hollow-knight"));
        QCOMPARE(m->reason, MatchReason::InstallDirName);

        m = matcher.match(install(QStringLiteral("epic"), QStringLiteral("Zzz"), QStringLiteral("/games/abc"), {},
                                  QStringLiteral("Other Game")));
        QCOMPARE(m->profileId, QStringLiteral("other-game"));
        QVERIFY(!matcher.match(install(QStringLiteral("epic"), QStringLiteral("Zzz"), QStringLiteral("/games/unknown"))));
    }

    void firstProfileWinsStoreIdConflicts()
    {
        profiles::Identifiers ids;
        ids.steam = {1};
        const GameMatcher matcher({profile(QStringLiteral("local-override"), ids), profile(QStringLiteral("upstream"), ids)});
        QCOMPARE(matcher.match(install(QStringLiteral("steam"), QStringLiteral("1"), QStringLiteral("/a")))->profileId,
                 QStringLiteral("local-override"));
    }

    void normalizesNames()
    {
        QCOMPARE(GameMatcher::normalizeName(QStringLiteral("Hollow Knight: Silksong")), QStringLiteral("hollowknightsilksong"));
        QCOMPARE(GameMatcher::normalizeName(QStringLiteral("hollow_knight")), QStringLiteral("hollowknight"));
    }

    void detectGamesCombinesInstallsAndSaveFirstScan()
    {
        QTemporaryDir tmp;
        DetectionEnvironment env;
        env.hostOs = Os::Windows;
        env.home = tmp.path();
        env.registry = std::make_shared<MapRegistryReader>();
        env.userFolders.set(QStringLiteral("home"), tmp.path());
        env.userFolders.set(QStringLiteral("winAppData"), tmp.path() + QStringLiteral("/AppData/Roaming"));

        // "Portable" game: not installed through any launcher, but its saves exist.
        const QString saves = tmp.path() + QStringLiteral("/AppData/Roaming/Indie Studio/Portable Game");
        QVERIFY(QDir().mkpath(saves));
        QFile save(saves + QStringLiteral("/slot1.sav"));
        QVERIFY(save.open(QIODevice::WriteOnly));
        save.write("progress");
        save.close();

        QList<profiles::Profile> profiles = catalog();
        profiles::Identifiers portableIds;
        portableIds.installDirNames = {QStringLiteral("Portable Game")};
        profiles.append(profile(QStringLiteral("portable-game"), portableIds,
                                {QStringLiteral("<winAppData>/Indie Studio/Portable Game/*.sav")}));

        const QList<InstallCandidate> installs{
            install(QStringLiteral("steam"), QStringLiteral("367520"), tmp.path() + QStringLiteral("/Steam/Hollow Knight")),
            install(QStringLiteral("steam"), QStringLiteral("999999"), tmp.path() + QStringLiteral("/Steam/Unknown Thing")),
        };

        const DetectionReport report = detectGames(profiles, installs, env);
        QCOMPARE(report.games.size(), 2);
        QCOMPARE(report.games.at(0).profileId, QStringLiteral("hollow-knight"));
        QVERIFY(report.games.at(0).install.has_value());
        QCOMPARE(report.games.at(1).profileId, QStringLiteral("portable-game"));
        QCOMPARE(report.games.at(1).reason, MatchReason::SavesFound);
        QCOMPARE(report.games.at(1).saveFilesFound, 1);
        QVERIFY(!report.games.at(1).install.has_value());
        QCOMPARE(report.unmatched.size(), 1);
        QCOMPARE(report.unmatched.first().storeId, QStringLiteral("999999"));

        QCOMPARE(detectGames(profiles, installs, env, false).games.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestGameMatcher)
#include "tst_game_matcher.moc"
