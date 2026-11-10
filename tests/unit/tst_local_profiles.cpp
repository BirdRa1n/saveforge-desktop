#include "ServiceFixture.h"

#include "savevault/services/Library.h"
#include "savevault/services/LocalProfiles.h"

#include <QFile>

using namespace savevault;
using namespace savevault::services;
using namespace savevault::testing;

namespace {

platform::FolderMap userFolders(const ServiceFixture &f)
{
    platform::FolderMap folders;
    folders.set(QStringLiteral("home"), f.root.path());
    folders.set(QStringLiteral("winAppData"), f.appData());
    return folders;
}

detection::DetectionEnvironment environment(const ServiceFixture &f)
{
    detection::DetectionEnvironment env;
    env.hostOs = platform::Os::Windows;
    env.home = f.root.path();
    env.registry = std::make_shared<detection::MapRegistryReader>();
    env.userFolders = userFolders(f);
    return env;
}

QString describe(const Result<profiles::Profile> &result)
{
    return result.ok() ? QString() : result.error().toString();
}

} // namespace

class TestLocalProfiles : public QObject {
    Q_OBJECT

private slots:
    void createsAManualGameFromUserFolders()
    {
        ServiceFixture f;
        QTemporaryDir dir;
        LocalProfileStore store(dir.path());
        // The deepest user folder wins: AppData beats home.
        const auto added = store.addManualGame(QStringLiteral("  Café: The Game  "),
                                               {QDir::toNativeSeparators(f.appData() + QStringLiteral("/Cafe Studio/Saves"))},
                                               userFolders(f), platform::Os::Windows);
        QVERIFY2(added.ok(), qPrintable(describe(added)));
        QCOMPARE(added.value().id, QStringLiteral("local-cafe-the-game"));
        QCOMPARE(added.value().name, QStringLiteral("Café: The Game"));
        QCOMPARE(added.value().saves.first().paths, QStringList{QStringLiteral("<winAppData>/Cafe Studio/Saves")});
        QCOMPARE(added.value().provenance.source, QStringLiteral("local"));
        QVERIFY(QFile::exists(dir.path() + QStringLiteral("/local-cafe-the-game.json")));

        // Same name again: a new id, never an overwrite.
        const auto again = store.addManualGame(QStringLiteral("Café: The Game"), {f.root.path() + QStringLiteral("/Other")},
                                               userFolders(f), platform::Os::Windows);
        QVERIFY2(again.ok(), qPrintable(describe(again)));
        QCOMPARE(again.value().id, QStringLiteral("local-cafe-the-game-2"));
        QCOMPARE(again.value().saves.first().paths, QStringList{QStringLiteral("<home>/Other")});

        const LocalProfileStore::Load load = store.load();
        QCOMPARE(load.profiles.size(), 2);
        QVERIFY(load.warnings.isEmpty());
    }

    void refusesFoldersProfilesCannotDescribe()
    {
        ServiceFixture f;
        QTemporaryDir dir;
        QTemporaryDir elsewhere;
        LocalProfileStore store(dir.path());
        const platform::FolderMap folders = userFolders(f);

        QCOMPARE(store.addManualGame(QString(), {f.appData() + QStringLiteral("/X")}, folders).error().code,
                 ErrorCode::InvalidArgument);
        QCOMPARE(store.addManualGame(QStringLiteral("X"), {}, folders).error().code, ErrorCode::InvalidArgument);
        // Outside every user folder: profiles have no absolute paths.
        QCOMPARE(store.addManualGame(QStringLiteral("X"), {elsewhere.path()}, folders, platform::Os::Windows).error().code,
                 ErrorCode::InvalidArgument);
        // A whole user folder is rejected by the path rules.
        QCOMPARE(store.addManualGame(QStringLiteral("X"), {f.appData()}, folders, platform::Os::Windows).error().code,
                 ErrorCode::InvalidArgument);
        QVERIFY(store.load().profiles.isEmpty());
    }

    void importsRemovesAndSkipsInvalidFiles()
    {
        QTemporaryDir dir;
        LocalProfileStore store(dir.path());
        const QString example = QStringLiteral(SAVEVAULT_SPECS_DIR "/profile/examples/profile.v1/valid");
        const QStringList files = QDir(example).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        QVERIFY(!files.isEmpty());
        const auto imported = store.importProfile(example + u'/' + files.first());
        QVERIFY2(imported.ok(), qPrintable(describe(imported)));
        QCOMPARE(store.load().profiles.size(), 1);

        writeFile(dir.path() + QStringLiteral("/broken.json"), "{");
        // A valid profile whose file name does not match its id.
        QVERIFY(QFile::copy(example + u'/' + files.first(), dir.path() + QStringLiteral("/renamed.json")));
        const LocalProfileStore::Load load = store.load();
        QCOMPARE(load.profiles.size(), 1);
        QCOMPARE(load.warnings.size(), 2);

        QVERIFY(store.remove(imported.value().id).ok());
        QCOMPARE(store.remove(imported.value().id).error().code, ErrorCode::NotFound);
        QCOMPARE(store.remove(QStringLiteral("../escape")).error().code, ErrorCode::NotFound);
        QCOMPARE(store.importProfile(dir.path() + QStringLiteral("/broken.json")).error().code, ErrorCode::InvalidArgument);
    }

    void manualGamesAppearInTheLibrary()
    {
        ServiceFixture f;
        QTemporaryDir cache;
        QTemporaryDir dir;
        LocalProfileStore store(dir.path());
        writeFile(f.appData() + QStringLiteral("/Indie/slot1.sav"), "s");
        QVERIFY(store.addManualGame(QStringLiteral("Indie"), {f.appData() + QStringLiteral("/Indie")}, userFolders(f),
                                    platform::Os::Windows)
                    .ok());

        const CatalogLoad load = loadCatalogFromCache(cache.path(), QStringLiteral("0.1.0"), dir.path());
        QCOMPARE(load.catalog.size(), 1);
        QCOMPARE(load.catalog.entries().first().channel, QStringLiteral("local"));

        const LibraryScan scan = scanLibrary(load.catalog, {}, environment(f), nullptr);
        QCOMPARE(scan.games.size(), 1);
        QCOMPARE(scan.games.first().id(), QStringLiteral("local-indie"));
        QCOMPARE(scan.games.first().saveFilesFound, 1);
    }
};

QTEST_GUILESS_MAIN(TestLocalProfiles)
#include "tst_local_profiles.moc"
