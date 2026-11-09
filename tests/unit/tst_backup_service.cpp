#include "ServiceFixture.h"

#include "saveforge/services/BackupService.h"

using namespace saveforge;
using namespace saveforge::services;
using namespace saveforge::testing;

class TestBackupService : public QObject {
    Q_OBJECT

private slots:
    void backsUpResolvedFiles()
    {
        ServiceFixture f;
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "slot one");
        writeFile(f.saveDir() + QStringLiteral("/slot2.sav"), "slot two");
        writeFile(f.saveDir() + QStringLiteral("/settings.ini"), "not matched by the profile");

        BackupService service(*f.vault, f.environment());
        BackupOptions options;
        options.label = QStringLiteral("before the boss");
        const auto snapshot = service.backup(f.target(), options);
        QVERIFY2(snapshot.ok(), qPrintable(snapshot.ok() ? QString() : snapshot.error().toString()));

        const vault::Snapshot &s = snapshot.value();
        QCOMPARE(s.gameId, QStringLiteral("fictional-game"));
        QCOMPARE(s.label, QStringLiteral("before the boss"));
        QCOMPARE(s.profile.revision, 2);
        QCOMPARE(s.files.size(), 2);
        QCOMPARE(s.files.first().path, QStringLiteral("<winAppData>/Fictional Studio/Fictional Game/slot1.sav"));
        QCOMPARE(s.files.first().ruleId, QStringLiteral("progress"));
    }

    void reportsMissingSaves()
    {
        ServiceFixture f;
        BackupService service(*f.vault, f.environment());
        const auto result = service.backup(f.target());
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::NotFound);
    }

    void refusesWhileRunningWhenProfileRequiresIt()
    {
        ServiceFixture f;
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "slot one");
        f.processes = {{1234, QStringLiteral("fictionalgame.EXE"), QStringLiteral("C:/x/fictionalgame.exe")}};
        BackupService service(*f.vault, f.environment());

        const auto refused = service.backup(f.target(/*requireGameClosed=*/true));
        QVERIFY(!refused.ok());
        QCOMPARE(refused.error().code, ErrorCode::PermissionDenied);

        // Profiles that tolerate a running game still back up (the vault
        // re-reads changing files and marks the snapshot if needed).
        QVERIFY(service.backup(f.target(false)).ok());
    }

    void detectsGamesByInstallFolder()
    {
        ServiceFixture f;
        const GameTarget target = f.target();
        QVERIFY(!isGameRunning(target, {{1, QStringLiteral("explorer.exe"), QStringLiteral("C:/Windows/explorer.exe")}}));
        QVERIFY(isGameRunning(target, {{2, QStringLiteral("launcher.exe"),
                                        f.installDir().toUpper() + QStringLiteral("/bin/launcher.exe")}}));
        // A sibling folder that merely shares the prefix is not the game.
        QVERIFY(!isGameRunning(target, {{3, QStringLiteral("x.exe"), f.installDir() + QStringLiteral(" 2/x.exe")}}));
    }
};

QTEST_GUILESS_MAIN(TestBackupService)
#include "tst_backup_service.moc"
