#include "ServiceFixture.h"

#include "savevault/services/BackupService.h"
#include "savevault/services/RestoreService.h"

#include <QDirIterator>

using namespace savevault;
using namespace savevault::services;
using namespace savevault::testing;

namespace {

// Every file under `dir` whose name marks it as a restore leftover.
QStringList leftovers(const QString &dir)
{
    QStringList found;
    QDirIterator it(dir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (path.contains(QLatin1String(".savevault-")))
            found.append(path);
    }
    return found;
}

} // namespace

class TestRestoreService : public QObject {
    Q_OBJECT

private:
    // slot1 and slot2 backed up; returns the snapshot.
    static vault::Snapshot backupTwoSlots(ServiceFixture &f)
    {
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "slot one, chapter 3");
        writeFile(f.saveDir() + QStringLiteral("/slot2.sav"), "slot two, chapter 1");
        auto snapshot = BackupService(*f.vault, f.environment()).backup(f.target());
        if (!snapshot.ok())
            qFatal("backup failed: %s", qPrintable(snapshot.error().toString()));
        return snapshot.value();
    }

private slots:
    void plansWithoutTouchingFiles()
    {
        ServiceFixture f;
        const vault::Snapshot snapshot = backupTwoSlots(f);
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "slot one, chapter 9 (newer)");
        QVERIFY(QFile::remove(f.saveDir() + QStringLiteral("/slot2.sav")));
        writeFile(f.saveDir() + QStringLiteral("/slot3.sav"), "a new slot created after the backup");

        const auto plan = RestoreService(*f.vault, f.environment()).plan(f.target(), snapshot.snapshotId);
        QVERIFY2(plan.ok(), qPrintable(plan.ok() ? QString() : plan.error().toString()));
        QCOMPARE(plan.value().count(FileAction::Overwrite), 1);
        QCOMPARE(plan.value().count(FileAction::Create), 1);
        QCOMPARE(plan.value().extraFiles, QStringList{f.saveDir() + QStringLiteral("/slot3.sav")});
        QVERIFY(!plan.value().gameRunning);
        QVERIFY(!plan.value().launcherCloudConflict);
        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot1.sav")), QByteArray("slot one, chapter 9 (newer)"));
    }

    void restoresAndKeepsAnUndoPoint()
    {
        ServiceFixture f;
        const vault::Snapshot snapshot = backupTwoSlots(f);
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "slot one, chapter 9 (newer)");
        QVERIFY(QFile::remove(f.saveDir() + QStringLiteral("/slot2.sav")));
        writeFile(f.saveDir() + QStringLiteral("/slot3.sav"), "a new slot created after the backup");

        RestoreService service(*f.vault, f.environment());
        const auto plan = service.plan(f.target(), snapshot.snapshotId);
        const auto report = service.execute(f.target(), plan.value(), CloudPolicy::Ask);
        QVERIFY2(report.ok(), qPrintable(report.ok() ? QString() : report.error().toString()));
        QCOMPARE(report.value().written, 2);

        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot1.sav")), QByteArray("slot one, chapter 3"));
        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot2.sav")), QByteArray("slot two, chapter 1"));
        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot3.sav")), QByteArray("a new slot created after the backup"));
        QCOMPARE(QFileInfo(f.saveDir() + QStringLiteral("/slot1.sav")).lastModified(QTimeZone::UTC),
                 snapshot.files.first().mtime);
        QVERIFY(leftovers(f.root.path()).isEmpty());

        // The undo point holds the files as they were before the restore.
        QVERIFY(report.value().preRestoreSnapshotId.has_value());
        const auto undo = f.vault->snapshot(QStringLiteral("fictional-game"), *report.value().preRestoreSnapshotId);
        QVERIFY(undo.ok());
        QCOMPARE(undo.value().kind, vault::SnapshotKind::PreRestore);
        QCOMPARE(undo.value().files.size(), 2); // slot1 (chapter 9) and slot3
    }

    void doesNothingWhenFilesAlreadyMatch()
    {
        ServiceFixture f;
        const vault::Snapshot snapshot = backupTwoSlots(f);
        RestoreService service(*f.vault, f.environment());
        const auto plan = service.plan(f.target(), snapshot.snapshotId);
        QCOMPARE(plan.value().count(FileAction::Unchanged), 2);
        QVERIFY(!plan.value().hasChanges());
        const auto report = service.execute(f.target(), plan.value(), CloudPolicy::Ask);
        QVERIFY(report.ok());
        QCOMPARE(report.value().written, 0);
        QVERIFY(!report.value().preRestoreSnapshotId.has_value());
        QCOMPARE(f.vault->snapshots(QStringLiteral("fictional-game")).size(), 1);
    }

    void refusesWhileTheGameRuns()
    {
        ServiceFixture f;
        const vault::Snapshot snapshot = backupTwoSlots(f);
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "changed");
        RestoreService service(*f.vault, f.environment());
        const auto plan = service.plan(f.target(), snapshot.snapshotId);

        f.processes = {{77, QStringLiteral("FictionalGame.exe"), f.installDir() + QStringLiteral("/FictionalGame.exe")}};
        const auto report = service.execute(f.target(), plan.value(), CloudPolicy::PreferSaveVault);
        QVERIFY(!report.ok());
        QCOMPARE(report.error().code, ErrorCode::PermissionDenied);
        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot1.sav")), QByteArray("changed"));
    }

    void appliesTheLauncherCloudPolicy()
    {
        ServiceFixture f;
        const vault::Snapshot snapshot = backupTwoSlots(f);
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "changed");
        GameTarget target = f.target();
        target.profile.backup.cloudSync = {QStringLiteral("steam")};

        RestoreService service(*f.vault, f.environment());
        const auto plan = service.plan(target, snapshot.snapshotId);
        QVERIFY(plan.value().launcherCloudConflict);

        QCOMPARE(service.execute(target, plan.value(), CloudPolicy::Ask).error().code, ErrorCode::InvalidArgument);
        QCOMPARE(service.execute(target, plan.value(), CloudPolicy::PreferLauncher).error().code,
                 ErrorCode::PermissionDenied);
        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot1.sav")), QByteArray("changed"));

        QVERIFY(service.execute(target, plan.value(), CloudPolicy::PreferSaveVault).ok());
        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot1.sav")), QByteArray("slot one, chapter 3"));

        // Another store's cloud does not conflict with a Steam install.
        target.profile.backup.cloudSync = {QStringLiteral("gog")};
        QVERIFY(!service.plan(target, snapshot.snapshotId).value().launcherCloudConflict);
    }

    void isAllOrNothingWhenABlobIsCorrupted()
    {
        ServiceFixture f;
        const vault::Snapshot snapshot = backupTwoSlots(f);
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "current slot one");
        writeFile(f.saveDir() + QStringLiteral("/slot2.sav"), "current slot two");
        // Corrupt the blob of the second file only.
        writeFile(f.vault->blobs().blobPath(snapshot.files.at(1).sha256), "garbage");

        RestoreService service(*f.vault, f.environment());
        const auto plan = service.plan(f.target(), snapshot.snapshotId);
        const auto report = service.execute(f.target(), plan.value(), CloudPolicy::Ask);
        QVERIFY(!report.ok());
        QCOMPARE(report.error().code, ErrorCode::Corrupted);
        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot1.sav")), QByteArray("current slot one"));
        QCOMPARE(readFile(f.saveDir() + QStringLiteral("/slot2.sav")), QByteArray("current slot two"));
        QVERIFY(leftovers(f.root.path()).isEmpty());
    }

    void restoresIntoAnotherStoreAccount()
    {
        ServiceFixture f;
        GameTarget target = f.target();
        target.profile.saves.first().paths = {QStringLiteral("<winAppData>/Fictional Studio/<storeUserId>/*.sav")};
        target.context.storeUserIds = {QStringLiteral("111")};
        writeFile(f.appData() + QStringLiteral("/Fictional Studio/111/slot.sav"), "account 111 progress");
        const auto snapshot = BackupService(*f.vault, f.environment()).backup(target);
        QVERIFY(snapshot.ok());
        QCOMPARE(snapshot.value().files.first().bindings.value(QStringLiteral("storeUserId")), QStringLiteral("111"));

        target.context.storeUserIds = {QStringLiteral("222")};
        RestoreService service(*f.vault, f.environment());
        const auto plan = service.plan(target, snapshot.value().snapshotId, {{QStringLiteral("storeUserId"), QStringLiteral("222")}});
        QCOMPARE(plan.value().files.first().target, f.appData() + QStringLiteral("/Fictional Studio/222/slot.sav"));
        QVERIFY(service.execute(target, plan.value(), CloudPolicy::Ask).ok());
        QCOMPARE(readFile(f.appData() + QStringLiteral("/Fictional Studio/222/slot.sav")), QByteArray("account 111 progress"));
    }

    void skipsFilesThatCannotBeResolvedHere()
    {
        ServiceFixture f;
        const vault::Snapshot snapshot = backupTwoSlots(f);
        GameTarget target = f.target();
        target.context.folders.remove(QStringLiteral("winAppData")); // e.g. restoring on another OS
        const auto plan = RestoreService(*f.vault, f.environment()).plan(target, snapshot.snapshotId);
        QVERIFY(plan.ok());
        QCOMPARE(plan.value().count(FileAction::Skip), 2);
        QVERIFY(!plan.value().files.first().skipReason.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestRestoreService)
#include "tst_restore_service.moc"
