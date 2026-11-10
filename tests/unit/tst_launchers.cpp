// Launcher detectors against fixture trees that mimic each launcher's layout.
// Every OS is simulated through DetectionEnvironment, so all tests run on any host.

#include "savevault/detection/Launchers.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using namespace savevault;
using namespace savevault::detection;
using savevault::platform::Os;

namespace {

void write(const QString &path, const QByteArray &content = {})
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
}

void mkdir(const QString &path)
{
    QVERIFY(QDir().mkpath(path));
}

DetectionEnvironment environment(const QString &home, Os os)
{
    DetectionEnvironment env;
    env.hostOs = os;
    env.home = home;
    env.osUserName = QStringLiteral("player");
    env.registry = std::make_shared<MapRegistryReader>();
    env.userFolders.set(QStringLiteral("home"), home);
    if (os == Os::Windows) {
        env.userFolders.set(QStringLiteral("winAppData"), home + QStringLiteral("/AppData/Roaming"));
        env.userFolders.set(QStringLiteral("winProgramData"), home + QStringLiteral("/ProgramData"));
    }
    return env;
}

QByteArray appManifest(const char *appId, const char *name, const char *installDir)
{
    return QByteArray("\"AppState\"\n{\n\t\"appid\"\t\"") + appId + "\"\n\t\"name\"\t\"" + name
           + "\"\n\t\"installdir\"\t\"" + installDir + "\"\n}\n";
}

const InstallCandidate *find(const QList<InstallCandidate> &games, const QString &storeId)
{
    for (const InstallCandidate &game : games) {
        if (game.storeId == storeId)
            return &game;
    }
    return nullptr;
}

} // namespace

class TestLaunchers : public QObject {
    Q_OBJECT

private slots:
    void steamOnWindowsFindsAllLibraries()
    {
        QTemporaryDir tmp;
        const QString steam = tmp.path() + QStringLiteral("/Steam");
        const QString library2 = tmp.path() + QStringLiteral("/SteamLibrary");
        DetectionEnvironment env = environment(tmp.path() + QStringLiteral("/home"), Os::Windows);
        auto registry = std::make_shared<MapRegistryReader>();
        registry->set(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QStringLiteral("SteamPath"), steam);
        env.registry = registry;

        // Windows paths in libraryfolders.vdf are backslash-escaped.
        QString libraryPath = QDir::toNativeSeparators(library2);
        libraryPath.replace(u'\\', QLatin1String("\\\\"));
        write(steam + QStringLiteral("/steamapps/libraryfolders.vdf"),
              "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\"" + QDir::toNativeSeparators(steam).replace(u'\\', QLatin1String("\\\\")).toUtf8()
                  + "\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\"" + libraryPath.toUtf8() + "\"\n\t}\n}\n");
        write(steam + QStringLiteral("/steamapps/appmanifest_367520.acf"), appManifest("367520", "Hollow Knight", "Hollow Knight"));
        write(steam + QStringLiteral("/steamapps/common/Hollow Knight/hollow_knight.exe"));
        write(library2 + QStringLiteral("/steamapps/appmanifest_504230.acf"), appManifest("504230", "Celeste", "Celeste"));
        write(library2 + QStringLiteral("/steamapps/common/Celeste/Celeste.exe"));
        // Not installed (no directory) and a Steam tool: both ignored.
        write(steam + QStringLiteral("/steamapps/appmanifest_1.acf"), appManifest("1", "Ghost", "Ghost"));
        write(steam + QStringLiteral("/steamapps/appmanifest_228980.acf"),
              appManifest("228980", "Steamworks Common Redistributables", "Steamworks Shared"));
        mkdir(steam + QStringLiteral("/steamapps/common/Steamworks Shared"));
        mkdir(steam + QStringLiteral("/userdata/12345678"));

        const QList<InstallCandidate> games = detectSteam(env);
        QCOMPARE(games.size(), 2);
        const InstallCandidate *hk = find(games, QStringLiteral("367520"));
        QVERIFY(hk);
        QCOMPARE(hk->store, QStringLiteral("steam"));
        QCOMPARE(hk->name, QStringLiteral("Hollow Knight"));
        QCOMPARE(hk->gameOs, Os::Windows);
        QCOMPARE(hk->executables, QStringList{QStringLiteral("hollow_knight.exe")});
        QCOMPARE(hk->storeUserIds, (QStringList{QStringLiteral("12345678"), QStringLiteral("76561197972611406")}));
        QCOMPARE(find(games, QStringLiteral("504230"))->installDir, library2 + QStringLiteral("/steamapps/common/Celeste"));
    }

    void steamRootsAreDeduplicatedIgnoringCaseOnWindows()
    {
        QTemporaryDir tmp;
        const QString steam = tmp.path() + QStringLiteral("/Program Files (x86)/Steam");
        mkdir(steam + QStringLiteral("/steamapps"));
        DetectionEnvironment env = environment(tmp.path(), Os::Windows);
        auto registry = std::make_shared<MapRegistryReader>();
        // Real-world case: HKCU stores a lower-cased copy of the path.
        registry->set(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QStringLiteral("SteamPath"), steam.toLower());
        registry->set(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam"), QStringLiteral("InstallPath"),
                      QDir::toNativeSeparators(steam));
        env.registry = registry;

        if (!QFileInfo::exists(steam.toLower()))
            QSKIP("file system is case-sensitive");
        QCOMPARE(steamRoots(env), QStringList{steam}); // one root, with the real spelling
    }

    void steamOnLinuxDetectsProtonPrefixes()
    {
        QTemporaryDir tmp;
        const QString home = tmp.path();
        const QString steamapps = home + QStringLiteral("/.local/share/Steam/steamapps");
        write(steamapps + QStringLiteral("/appmanifest_367520.acf"), appManifest("367520", "Hollow Knight", "Hollow Knight"));
        write(steamapps + QStringLiteral("/common/Hollow Knight/hollow_knight.exe"));
        mkdir(steamapps + QStringLiteral("/compatdata/367520/pfx/drive_c/users/steamuser"));
        mkdir(steamapps + QStringLiteral("/compatdata/367520/pfx/drive_c/users/Public"));
        // Native Linux game without a prefix.
        write(steamapps + QStringLiteral("/appmanifest_504230.acf"), appManifest("504230", "Celeste", "Celeste"));
        mkdir(steamapps + QStringLiteral("/common/Celeste"));

        const DetectionEnvironment env = environment(home, Os::Linux);
        const QList<InstallCandidate> games = detectSteam(env);
        QCOMPARE(games.size(), 2);

        const InstallCandidate *hk = find(games, QStringLiteral("367520"));
        QCOMPARE(hk->gameOs, Os::Windows);
        QCOMPARE(hk->winePrefix, steamapps + QStringLiteral("/compatdata/367520/pfx"));
        QCOMPARE(hk->wineUser, QStringLiteral("steamuser"));
        const InstallCandidate *celeste = find(games, QStringLiteral("504230"));
        QCOMPARE(celeste->gameOs, Os::Linux);
        QVERIFY(celeste->winePrefix.isEmpty());

        // Paths for the Proton game resolve inside the prefix, with <base> = install dir.
        const auto context = resolveContextFor(*hk, env);
        QCOMPARE(context.folders.value(QStringLiteral("winAppData")),
                 steamapps + QStringLiteral("/compatdata/367520/pfx/drive_c/users/steamuser/AppData/Roaming"));
        QCOMPARE(context.folders.value(QStringLiteral("base")), hk->installDir);
        QCOMPARE(context.gameOs, Os::Windows);
        QVERIFY(context.caseInsensitive);
        QVERIFY(!resolveContextFor(*celeste, env).caseInsensitive);
    }

    void epicReadsManifestsAndSkipsDlcAndIncompleteInstalls()
    {
        QTemporaryDir tmp;
        const DetectionEnvironment env = environment(tmp.path(), Os::Windows);
        const QString manifests = tmp.path() + QStringLiteral("/ProgramData/Epic/EpicGamesLauncher/Data/Manifests");
        const QString hades = tmp.path() + QStringLiteral("/Games/Hades");
        write(hades + QStringLiteral("/x64/Hades.exe"));
        const auto item = [&](const char *app, const char *main, bool incomplete) {
            return QJsonDocument(QJsonObject{{QStringLiteral("AppName"), QLatin1String(app)},
                                             {QStringLiteral("MainGameAppName"), QLatin1String(main)},
                                             {QStringLiteral("DisplayName"), QStringLiteral("Hades")},
                                             {QStringLiteral("InstallLocation"), QDir::toNativeSeparators(hades)},
                                             {QStringLiteral("LaunchExecutable"), QStringLiteral("x64/Hades.exe")},
                                             {QStringLiteral("bIsIncompleteInstall"), incomplete}})
                .toJson();
        };
        write(manifests + QStringLiteral("/A.item"), item("Min", "Min", false));
        write(manifests + QStringLiteral("/B.item"), item("MinDLC", "Min", false));
        write(manifests + QStringLiteral("/C.item"), item("Other", "Other", true));
        write(manifests + QStringLiteral("/D.item"), "{ not json");

        const QList<InstallCandidate> games = detectEpic(env);
        QCOMPARE(games.size(), 1);
        QCOMPARE(games.first().storeId, QStringLiteral("Min"));
        QCOMPARE(games.first().store, QStringLiteral("epic"));
        QCOMPARE(games.first().installDir, hades);
        QCOMPARE(games.first().executables, QStringList{QStringLiteral("Hades.exe")});
    }

    void gogReadsBothRegistryViews()
    {
        QTemporaryDir tmp;
        DetectionEnvironment env = environment(tmp.path(), Os::Windows);
        const QString dir = tmp.path() + QStringLiteral("/GOG Games/Stardew Valley");
        write(dir + QStringLiteral("/Stardew Valley.exe"));
        auto registry = std::make_shared<MapRegistryReader>();
        const QString key = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games\\1453375253");
        registry->set(key, QStringLiteral("gameID"), QStringLiteral("1453375253"));
        registry->set(key, QStringLiteral("gameName"), QStringLiteral("Stardew Valley"));
        registry->set(key, QStringLiteral("path"), QDir::toNativeSeparators(dir));
        registry->set(key, QStringLiteral("exeFile"), QStringLiteral("Stardew Valley.exe"));
        // Same game also listed in the 32-bit view, and an entry whose folder is gone.
        registry->set(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\Games\\1453375253"), QStringLiteral("path"), dir);
        registry->set(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\Games\\999"), QStringLiteral("path"),
                      tmp.path() + QStringLiteral("/missing"));
        env.registry = registry;

        const QList<InstallCandidate> games = detectGog(env);
        QCOMPARE(games.size(), 1);
        QCOMPARE(games.first().storeId, QStringLiteral("1453375253"));
        QCOMPARE(games.first().name, QStringLiteral("Stardew Valley"));
        QCOMPARE(games.first().installDir, dir);

        QVERIFY(detectGog(environment(tmp.path(), Os::Linux)).isEmpty());
    }

    void heroicReadsEpicAndGogWithPrefixes()
    {
        QTemporaryDir tmp;
        const QString home = tmp.path();
        const QString config = home + QStringLiteral("/.var/app/com.heroicgameslauncher.hgl/config/heroic"); // Flatpak
        const QString hadesDir = home + QStringLiteral("/Games/Heroic/Hades");
        const QString prefix = home + QStringLiteral("/Games/Heroic/Prefixes/Hades");
        write(hadesDir + QStringLiteral("/Hades.exe"));
        mkdir(prefix + QStringLiteral("/pfx/drive_c/users/steamuser")); // Proton layout
        const QString gogDir = home + QStringLiteral("/Games/Heroic/Stardew Valley");
        mkdir(gogDir);

        write(config + QStringLiteral("/legendaryConfig/legendary/installed.json"),
              QJsonDocument(QJsonObject{{QStringLiteral("Min"), QJsonObject{{QStringLiteral("title"), QStringLiteral("Hades")},
                                                                             {QStringLiteral("install_path"), hadesDir},
                                                                             {QStringLiteral("platform"), QStringLiteral("Windows")}}}})
                  .toJson());
        write(config + QStringLiteral("/GamesConfig/Min.json"),
              QJsonDocument(QJsonObject{{QStringLiteral("Min"), QJsonObject{{QStringLiteral("winePrefix"), prefix}}}}).toJson());
        write(config + QStringLiteral("/gog_store/installed.json"),
              QJsonDocument(QJsonObject{{QStringLiteral("installed"),
                                         QJsonArray{QJsonObject{{QStringLiteral("appName"), QStringLiteral("1453375253")},
                                                                {QStringLiteral("install_path"), gogDir},
                                                                {QStringLiteral("platform"), QStringLiteral("linux")}}}}})
                  .toJson());

        const QList<InstallCandidate> games = detectHeroic(environment(home, Os::Linux));
        QCOMPARE(games.size(), 2);
        const InstallCandidate *hades = find(games, QStringLiteral("Min"));
        QCOMPARE(hades->store, QStringLiteral("epic"));
        QCOMPARE(hades->launcher, QStringLiteral("heroic"));
        QCOMPARE(hades->gameOs, Os::Windows);
        QCOMPARE(hades->winePrefix, prefix + QStringLiteral("/pfx"));
        QCOMPARE(hades->wineUser, QStringLiteral("steamuser"));
        const InstallCandidate *stardew = find(games, QStringLiteral("1453375253"));
        QCOMPARE(stardew->store, QStringLiteral("gog"));
        QCOMPARE(stardew->gameOs, Os::Linux);
        QCOMPARE(stardew->name, QStringLiteral("Stardew Valley")); // falls back to the folder name
    }

    void lutrisReadsGameFilesWithPrefixes()
    {
        QTemporaryDir tmp;
        const QString home = tmp.path();
        const QString games = home + QStringLiteral("/.config/lutris/games");
        // A Windows game in a Wine prefix (absolute exe), and a native one (relative exe).
        const QString prefix = home + QStringLiteral("/Games/hollow-knight");
        const QString hkDir = prefix + QStringLiteral("/drive_c/Games/Hollow Knight");
        write(hkDir + QStringLiteral("/hollow_knight.exe"));
        mkdir(prefix + QStringLiteral("/drive_c/users/player"));
        write(games + QStringLiteral("/hollow-knight-1699999999.yml"),
              "game:\n  exe: '" + hkDir.toUtf8() + "/hollow_knight.exe'\n  prefix: " + prefix.toUtf8()
                  + "\nsystem:\n  exe: ignored\nwine:\n  version: lutris-7.2\n");
        const QString nativeDir = home + QStringLiteral("/Games/celeste");
        write(nativeDir + QStringLiteral("/Celeste"));
        write(games + QStringLiteral("/celeste-1700000000.yml"),
              "game:\n  exe: Celeste\n  working_dir: \"" + nativeDir.toUtf8() + "\"\n");
        write(games + QStringLiteral("/broken-1.yml"), "game:\n  exe: /nowhere/x.exe\n");

        const QList<InstallCandidate> found = detectLutris(environment(home, Os::Linux));
        QCOMPARE(found.size(), 2);
        const auto byName = [&found](const QString &name) -> const InstallCandidate * {
            for (const InstallCandidate &c : found)
                if (c.name == name)
                    return &c;
            return nullptr;
        };
        const InstallCandidate *hk = byName(QStringLiteral("Hollow Knight"));
        QVERIFY(hk);
        QCOMPARE(hk->launcher, QStringLiteral("lutris"));
        QVERIFY(hk->store.isEmpty());
        QCOMPARE(hk->gameOs, Os::Windows);
        QCOMPARE(hk->installDir, hkDir);
        QCOMPARE(hk->winePrefix, prefix);
        QCOMPARE(hk->wineUser, QStringLiteral("player"));
        QVERIFY(hk->executables.contains(QStringLiteral("hollow_knight.exe")));
        const InstallCandidate *celeste = byName(QStringLiteral("Celeste"));
        QVERIFY(celeste);
        QCOMPARE(celeste->gameOs, Os::Linux);
        QCOMPARE(celeste->installDir, nativeDir);
        QVERIFY(celeste->executables.contains(QStringLiteral("Celeste")));

        // Lutris exists only on Linux.
        QVERIFY(detectLutris(environment(home, Os::Windows)).isEmpty());
    }

    void missingLaunchersYieldNothing()
    {
        QTemporaryDir tmp;
        for (Os os : {Os::Windows, Os::Linux, Os::MacOS})
            QVERIFY(detectInstalledGames(environment(tmp.path(), os)).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLaunchers)
#include "tst_launchers.moc"
