#include "ServiceFixture.h"

#include "saveforge/services/BackupService.h"
#include "saveforge/services/Library.h"

using namespace saveforge;
using namespace saveforge::services;
using namespace saveforge::testing;

namespace {

profiles::Profile makeProfile(const QString &id, const QString &name, const QString &savePath, QList<qint64> steam = {})
{
    profiles::Profile p;
    p.id = id;
    p.name = name;
    p.identifiers.steam = steam;
    profiles::SaveRule rule;
    rule.id = QStringLiteral("save");
    rule.tags = {profiles::Tag::Save};
    rule.paths = {savePath};
    p.saves = {rule};
    p.mechanism.type = QStringLiteral("standard");
    p.provenance.source = QStringLiteral("ludusavi");
    return p;
}

detection::DetectionEnvironment environment(const ServiceFixture &f)
{
    detection::DetectionEnvironment env;
    env.hostOs = platform::Os::Windows;
    env.home = f.root.path();
    env.registry = std::make_shared<detection::MapRegistryReader>();
    env.userFolders.set(QStringLiteral("home"), f.root.path());
    env.userFolders.set(QStringLiteral("winAppData"), f.appData());
    return env;
}

} // namespace

class TestLibrary : public QObject {
    Q_OBJECT

private slots:
    void combinesDetectionCatalogAndVault()
    {
        ServiceFixture f;
        const detection::DetectionEnvironment env = environment(f);

        // A: installed through Steam (matched by store id), Steam Cloud enabled.
        profiles::Profile a = makeProfile(QStringLiteral("alpha"), QStringLiteral("Alpha"),
                                          QStringLiteral("<winAppData>/Alpha/*.sav"), {100});
        a.backup.cloudSync = {QStringLiteral("steam")};
        writeFile(f.appData() + QStringLiteral("/Alpha/1.sav"), "a");
        // B: not installed, but its saves exist.
        writeFile(f.appData() + QStringLiteral("/Beta/1.sav"), "b");
        const profiles::Profile b = makeProfile(QStringLiteral("beta"), QStringLiteral("beta"), QStringLiteral("<winAppData>/Beta/*.sav"));
        // C: nothing on disk, but backed up earlier.
        const profiles::Profile c = makeProfile(QStringLiteral("gamma"), QStringLiteral("Gamma"), QStringLiteral("<winAppData>/Gamma/*.sav"));
        // D: nothing anywhere.
        const profiles::Profile d = makeProfile(QStringLiteral("delta"), QStringLiteral("Delta"), QStringLiteral("<winAppData>/Delta/*.sav"));

        // Back up Gamma while its saves existed, then delete them.
        writeFile(f.appData() + QStringLiteral("/Gamma/1.sav"), "g");
        GameTarget gammaTarget;
        gammaTarget.profile = c;
        gammaTarget.context = detection::hostResolveContext(env, {});
        QVERIFY(BackupService(*f.vault, f.environment()).backup(gammaTarget).ok());
        QVERIFY(QDir(f.appData() + QStringLiteral("/Gamma")).removeRecursively());

        registry::ProfileCatalog catalog;
        catalog.addChannel(QStringLiteral("upstream-ludusavi"), {a, b, c, d});

        detection::InstallCandidate alphaInstall;
        alphaInstall.store = QStringLiteral("steam");
        alphaInstall.storeId = QStringLiteral("100");
        alphaInstall.installDir = f.installDir();
        alphaInstall.gameOs = platform::Os::Windows;
        detection::InstallCandidate unknown = alphaInstall;
        unknown.storeId = QStringLiteral("999");

        const LibraryScan scan = scanLibrary(catalog, {alphaInstall, unknown}, env, &*f.vault);
        QStringList ids;
        for (const LibraryGame &game : scan.games)
            ids.append(game.id());
        QCOMPARE(ids, (QStringList{QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")})); // sorted by name, case-insensitive

        const LibraryGame &alpha = scan.games.at(0);
        QCOMPARE(alpha.reason, detection::MatchReason::StoreId);
        QVERIFY(alpha.install.has_value());
        QVERIFY(alpha.launcherCloud);
        QCOMPARE(alpha.target.context.folders.value(QStringLiteral("base")), f.installDir());
        QCOMPARE(alpha.target.channel, QStringLiteral("upstream-ludusavi"));

        const LibraryGame &beta = scan.games.at(1);
        QCOMPARE(beta.reason, detection::MatchReason::SavesFound);
        QCOMPARE(beta.saveFilesFound, 1);
        QVERIFY(!beta.lastSnapshot.has_value());

        const LibraryGame &gamma = scan.games.at(2);
        QCOMPARE(gamma.snapshotCount, 1);
        QVERIFY(gamma.lastSnapshot.has_value());

        QCOMPARE(scan.unmatched.size(), 1);
    }

    void preRestoreSnapshotsAreNotTheLastBackup()
    {
        ServiceFixture f;
        const detection::DetectionEnvironment env = environment(f);
        const profiles::Profile p = makeProfile(QStringLiteral("alpha"), QStringLiteral("Alpha"), QStringLiteral("<winAppData>/Alpha/*.sav"));
        writeFile(f.appData() + QStringLiteral("/Alpha/1.sav"), "a");
        GameTarget target;
        target.profile = p;
        target.context = detection::hostResolveContext(env, {});
        BackupService backup(*f.vault, f.environment());
        const auto manual = backup.backup(target);
        BackupOptions undo;
        undo.kind = vault::SnapshotKind::PreRestore;
        QVERIFY(manual.ok() && backup.backup(target, undo).ok());

        registry::ProfileCatalog catalog;
        catalog.addChannel(QStringLiteral("local"), {p});
        const LibraryScan scan = scanLibrary(catalog, {}, env, &*f.vault);
        QCOMPARE(scan.games.first().snapshotCount, 2);
        QCOMPARE(scan.games.first().lastSnapshot->snapshotId, manual.value().snapshotId);
    }

    void worksWithoutAVault()
    {
        ServiceFixture f;
        writeFile(f.appData() + QStringLiteral("/Alpha/1.sav"), "a");
        registry::ProfileCatalog catalog;
        catalog.addChannel(QStringLiteral("local"),
                           {makeProfile(QStringLiteral("alpha"), QStringLiteral("Alpha"), QStringLiteral("<winAppData>/Alpha/*.sav"))});
        const LibraryScan scan = scanLibrary(catalog, {}, environment(f), nullptr);
        QCOMPARE(scan.games.size(), 1);
        QCOMPARE(scan.games.first().snapshotCount, 0);
    }

    void emptyCacheMeansEmptyCatalog()
    {
        QTemporaryDir tmp;
        const CatalogLoad load = loadCatalogFromCache(tmp.path(), QStringLiteral("0.1.0"));
        QCOMPARE(load.catalog.size(), 0);
        QVERIFY(load.usingTestKeys);
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "tst_library.moc"
