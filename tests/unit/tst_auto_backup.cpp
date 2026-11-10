#include "ServiceFixture.h"

#include "saveforge/services/AutoBackup.h"
#include "saveforge/services/BackupService.h"

#include <QFile>

using namespace saveforge;
using namespace saveforge::services;
using namespace saveforge::testing;

class TestAutoBackup : public QObject {
    Q_OBJECT

private slots:
    void reportsGamesThatStoppedRunning()
    {
        ServiceFixture f;
        LibraryGame game;
        game.target = f.target();
        const platform::ProcessInfo gameProcess{10, QStringLiteral("FictionalGame.exe"), {}};
        const platform::ProcessInfo other{11, QStringLiteral("explorer.exe"), QStringLiteral("C:/Windows/explorer.exe")};

        GameExitTracker tracker;
        QVERIFY(tracker.update({game}, {other}).isEmpty());
        QVERIFY(tracker.update({game}, {other, gameProcess}).isEmpty()); // started
        QVERIFY(tracker.running().contains(game.id()));
        QVERIFY(tracker.update({game}, {other, gameProcess}).isEmpty()); // still running
        QCOMPARE(tracker.update({game}, {other}), QStringList{game.id()}); // exited
        QVERIFY(tracker.update({game}, {other}).isEmpty());               // reported once
    }

    void gamesRunningAtStartCountWhenTheyExit()
    {
        ServiceFixture f;
        LibraryGame game;
        game.target = f.target();
        GameExitTracker tracker;
        QVERIFY(tracker.update({game}, {{10, QStringLiteral("FictionalGame.exe"), {}}}).isEmpty());
        QCOMPARE(tracker.update({game}, {}), QStringList{game.id()});
    }

    void detectsChangedSaves()
    {
        ServiceFixture f;
        const GameTarget target = f.target();
        QVERIFY(!savesChangedSince(target, nullptr)); // no saves at all

        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "one");
        QVERIFY(savesChangedSince(target, nullptr)); // never backed up

        const auto snapshot = BackupService(*f.vault, f.environment()).backup(target);
        QVERIFY(snapshot.ok());
        QVERIFY(!savesChangedSince(target, &snapshot.value()));

        // A new file, a different size or a later modification time count as changes.
        writeFile(f.saveDir() + QStringLiteral("/slot2.sav"), "two");
        QVERIFY(savesChangedSince(target, &snapshot.value()));
        QFile::remove(f.saveDir() + QStringLiteral("/slot2.sav"));
        QVERIFY(!savesChangedSince(target, &snapshot.value()));

        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "one, longer");
        QVERIFY(savesChangedSince(target, &snapshot.value()));

        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "uno");
        QFile file(f.saveDir() + QStringLiteral("/slot1.sav"));
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(60), QFileDevice::FileModificationTime));
        file.close();
        QVERIFY(savesChangedSince(target, &snapshot.value()));
    }
};

QTEST_GUILESS_MAIN(TestAutoBackup)
#include "tst_auto_backup.moc"
