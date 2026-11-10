#include "ServiceFixture.h"

#include "saveforge/core/Uuid.h"

#include "saveforge/services/BackupService.h"
#include "saveforge/services/LocalProfiles.h"
#include "saveforge/services/RestoreService.h"
#include "saveforge/services/Transfer.h"

#include <QJsonDocument>

using namespace saveforge;
using namespace saveforge::services;
using namespace saveforge::testing;

namespace {

vault::Snapshot backup(ServiceFixture &f, const QString &slot = {})
{
    BackupOptions options;
    options.slot = slot;
    auto snapshot = BackupService(*f.vault, f.environment()).backup(f.target(), options);
    if (!snapshot.ok())
        qFatal("backup failed: %s", qPrintable(snapshot.error().toString()));
    return snapshot.value();
}

ExportRequest requestFor(ServiceFixture &f, const QString &path)
{
    ExportRequest request;
    request.path = path;
    request.games = {{f.target().effectiveGameId(), QStringLiteral("Fictional Game"), std::nullopt}};
    request.appVersion = QStringLiteral("0.1.0");
    request.sourceOs = QStringLiteral("windows");
    return request;
}

} // namespace

class TestTransfer : public QObject {
    Q_OBJECT

private slots:
    void movesBackupsToAnotherVault()
    {
        ServiceFixture source;
        writeFile(source.saveDir() + QStringLiteral("/slot1.sav"), "chapter 1");
        backup(source);
        writeFile(source.saveDir() + QStringLiteral("/slot1.sav"), "chapter 2");
        const vault::Snapshot newest = backup(source, QStringLiteral("Run 2"));

        QTemporaryDir out;
        ExportRequest request = requestFor(source, out.path() + QStringLiteral("/saves.gamesave"));
        request.allHistory = true;
        const auto record = exportGames(*source.vault, request);
        QVERIFY2(record.ok(), qPrintable(record.ok() ? QString() : record.error().toString()));
        QCOMPARE(record.value().gameIds, QStringList{source.target().effectiveGameId()});
        QCOMPARE(record.value().blobs.size(), 2);

        // Another machine: import, then restore through the normal procedure.
        ServiceFixture target;
        const auto reader = archive::GamesaveReader::open(request.path);
        QVERIFY(reader.ok());
        const auto report = importGamesave(*target.vault, reader.value());
        QVERIFY2(report.ok(), qPrintable(report.ok() ? QString() : report.error().toString()));
        QCOMPARE(report.value().imported, 2);
        QCOMPARE(target.vault->snapshots(target.target().effectiveGameId()).last().slot, QStringLiteral("Run 2"));

        // Importing again changes nothing.
        QCOMPARE(importGamesave(*target.vault, reader.value()).value().alreadyPresent, 2);

        RestoreService restore(*target.vault, target.environment());
        const auto plan = restore.plan(target.target(), newest.snapshotId);
        QVERIFY(plan.ok());
        QVERIFY(restore.execute(target.target(), plan.value(), CloudPolicy::Ask).ok());
        QCOMPARE(readFile(target.saveDir() + QStringLiteral("/slot1.sav")), QByteArray("chapter 2"));
    }

    void latestOnlyExportsOneSnapshotPerGame()
    {
        ServiceFixture f;
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "a");
        backup(f);
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "b");
        const vault::Snapshot newest = backup(f);
        QTemporaryDir out;
        const ExportRequest request = requestFor(f, out.path() + QStringLiteral("/latest.gamesave"));
        QVERIFY(exportGames(*f.vault, request).ok());
        const auto reader = archive::GamesaveReader::open(request.path);
        QVERIFY(reader.ok());
        QCOMPARE(reader.value().manifest().games.first().snapshots.size(), 1);
        QCOMPARE(reader.value().manifest().games.first().snapshots.first().snapshotId, newest.snapshotId);
    }

    void incrementalExportsNeedTheirBase()
    {
        ServiceFixture f;
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "unchanged");
        writeFile(f.saveDir() + QStringLiteral("/slot2.sav"), "first");
        backup(f);
        QTemporaryDir out;
        ExportRequest first = requestFor(f, out.path() + QStringLiteral("/first.gamesave"));
        const auto base = exportGames(*f.vault, first);
        QVERIFY(base.ok());

        writeFile(f.saveDir() + QStringLiteral("/slot2.sav"), "second");
        backup(f);
        ExportRequest second = requestFor(f, out.path() + QStringLiteral("/second.gamesave"));
        second.base = base.value();
        const auto incremental = exportGames(*f.vault, second);
        QVERIFY(incremental.ok());
        QCOMPARE(incremental.value().blobs.size(), 1); // only the changed file travels

        // Without the base, the incremental export is refused and nothing is added.
        ServiceFixture other;
        const auto secondReader = archive::GamesaveReader::open(second.path);
        QVERIFY(secondReader.ok());
        const auto refused = importGamesave(*other.vault, secondReader.value());
        QVERIFY(!refused.ok());
        QVERIFY(refused.error().message.contains(QUuid(base.value().exportId).toString(QUuid::WithoutBraces)));
        QVERIFY(other.vault->snapshots(other.target().effectiveGameId()).isEmpty());

        // With the base imported first, it works.
        QVERIFY(importGamesave(*other.vault, archive::GamesaveReader::open(first.path).value()).ok());
        QVERIFY(importGamesave(*other.vault, secondReader.value()).ok());
        QCOMPARE(other.vault->snapshots(other.target().effectiveGameId()).size(), 2);
    }

    void carriesLocalProfiles()
    {
        ServiceFixture f;
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "x");
        backup(f);
        QTemporaryDir out;
        QTemporaryDir profiles;
        // The fixture's game, described by a local profile only this machine has.
        // Plain literals: moc misreads raw strings with quotes (see CONTRIBUTING).
        const QByteArray profileJson =
            "{\"schemaVersion\":1,\"id\":\"fictional-game\",\"name\":\"Fictional Game\",\"revision\":1,"
            "\"identifiers\":{\"installDirNames\":[\"Fictional Game\"]},"
            "\"saves\":[{\"id\":\"progress\",\"tags\":[\"save\"],"
            "\"paths\":[\"<winAppData>/Fictional Studio/Fictional Game/*.sav\"]}],"
            "\"mechanism\":{\"type\":\"standard\"},"
            "\"provenance\":{\"source\":\"local\",\"license\":\"LicenseRef-Local\"}}";
        ExportRequest request = requestFor(f, out.path() + QStringLiteral("/local.gamesave"));
        request.games.first().localProfile = QJsonDocument::fromJson(profileJson).object();
        QVERIFY(exportGames(*f.vault, request).ok());

        ServiceFixture other;
        LocalProfileStore store(profiles.path());
        const auto report = importGamesave(*other.vault, archive::GamesaveReader::open(request.path).value(), &store);
        QVERIFY(report.ok());
        QCOMPARE(report.value().addedLocalProfiles, QStringList{QStringLiteral("Fictional Game")});
        QCOMPARE(store.load().profiles.size(), 1);

        // A machine that already knows the game keeps its own profile.
        QTemporaryDir otherProfiles;
        LocalProfileStore untouched(otherProfiles.path());
        ServiceFixture third;
        const auto known = importGamesave(*third.vault, archive::GamesaveReader::open(request.path).value(), &untouched,
                                          {QStringLiteral("fictional-game")});
        QVERIFY(known.ok());
        QVERIFY(known.value().addedLocalProfiles.isEmpty());
    }

    void refusesExportsWithoutBackups()
    {
        ServiceFixture f;
        QTemporaryDir out;
        const auto result = exportGames(*f.vault, requestFor(f, out.path() + QStringLiteral("/none.gamesave")));
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::NotFound);
    }

    void logsExports()
    {
        QTemporaryDir dir;
        ExportLog log(dir.path());
        QVERIFY(!log.latest());
        ExportRecord older{createUuidV7(), QDateTime::currentDateTimeUtc().addSecs(-60), {QStringLiteral("a")}, {QStringLiteral("h1")}, {}};
        ExportRecord newer{createUuidV7(), QDateTime::currentDateTimeUtc(), {QStringLiteral("a")}, {QStringLiteral("h2")}, {}};
        QVERIFY(log.add(newer).ok());
        QVERIFY(log.add(older).ok());
        QCOMPARE(log.latest()->exportId, newer.exportId);
        QCOMPARE(log.latest()->blobs, QSet<QString>{QStringLiteral("h2")});
    }
};

QTEST_GUILESS_MAIN(TestTransfer)
#include "tst_transfer.moc"
