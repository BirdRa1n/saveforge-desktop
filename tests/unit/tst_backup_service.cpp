#include "ServiceFixture.h"

#include "savevault/services/BackupService.h"

using namespace savevault;
using namespace savevault::services;
using namespace savevault::testing;

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

    void asksBeforeBackingUpMoreThanExpected()
    {
        ServiceFixture f;
        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), QByteArray(100, 'x'));
        GameTarget target = f.target();
        target.profile.backup.maxBytes = 50;
        BackupService service(*f.vault, f.environment());
        const auto refused = service.backup(target);
        QVERIFY(!refused.ok());
        QCOMPARE(refused.error().code, ErrorCode::TooLarge);
        QCOMPARE(refused.error().context, QStringLiteral("100"));
        BackupOptions options;
        options.allowLargerThanExpected = true;
        QVERIFY(service.backup(target, options).ok());
    }

    void rereadsConsistentGroupsTogether()
    {
        ServiceFixture f;
        const QString db = f.saveDir() + QStringLiteral("/world.sav");
        const QString journal = f.saveDir() + QStringLiteral("/world.sav-journal.sav");
        writeFile(db, "db v1");
        writeFile(journal, "journal v1");
        GameTarget target = f.target();
        target.profile.backup.consistentGroups = {{QStringLiteral("world.sav*")}};

        // The game rewrites the database while the journal is being read.
        bool changed = false;
        BackupOptions options;
        options.onFileRead = [&](const QString &path, qsizetype, qsizetype) {
            if (!changed && path == journal) {
                changed = true;
                QFile file(db);
                if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    file.write("db v2, written later");
                file.close();
                file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(5), QFileDevice::FileModificationTime);
            }
            return true;
        };
        const auto snapshot = BackupService(*f.vault, f.environment()).backup(target, options);
        QVERIFY(snapshot.ok());
        QVERIFY(changed);
        QCOMPARE(snapshot.value().consistency, vault::Consistency::Consistent);
        // The group was read again, so the snapshot holds the newer database.
        const auto dbFile = std::find_if(snapshot.value().files.cbegin(), snapshot.value().files.cend(),
                                         [](const vault::SnapshotFile &file) { return file.path.endsWith(u"/world.sav"); });
        QVERIFY(dbFile != snapshot.value().files.cend());
        QCOMPARE(dbFile->size, qint64(QByteArray("db v2, written later").size()));
    }

    void findsTheFoldersThatHoldTheSaves()
    {
        ServiceFixture f;
        GameTarget target = f.target();
        QDir(f.saveDir()).removeRecursively();
        QVERIFY(saveFolders(target).isEmpty()); // nothing on disk yet

        QVERIFY(QDir().mkpath(f.saveDir()));
        QCOMPARE(saveFolders(target), QStringList{f.saveDir()});

        // A root-only rule (all of AppData) and a nested rule add nothing new;
        // a rule naming one file contributes the file's folder.
        profiles::SaveRule extra;
        extra.id = QStringLiteral("config");
        extra.tags = {profiles::Tag::Config};
        extra.paths = {QStringLiteral("<winAppData>/*.cfg"),
                       QStringLiteral("<winAppData>/Fictional Studio/Fictional Game/Profiles/<storeUserId>/*.sav"),
                       QStringLiteral("<winAppData>/Other Studio/settings.ini")};
        target.profile.saves.append(extra);
        writeFile(f.appData() + QStringLiteral("/Other Studio/settings.ini"), "x");
        // Folders that hold files the profile matches come first: only
        // settings.ini exists so far.
        QCOMPARE(saveFolders(target), (QStringList{f.appData() + QStringLiteral("/Other Studio"), f.saveDir()}));

        writeFile(f.saveDir() + QStringLiteral("/slot1.sav"), "x");
        QDir().remove(f.appData() + QStringLiteral("/Other Studio/settings.ini"));
        QCOMPARE(saveFolders(target).first(), f.saveDir());
    }
};

QTEST_GUILESS_MAIN(TestBackupService)
#include "tst_backup_service.moc"
