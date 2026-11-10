#include "savevault/core/Uuid.h"
#include "savevault/vault/Vault.h"

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTest>

using namespace savevault;
using namespace savevault::vault;

namespace {

const QUuid kMachine = QUuid::fromString(QStringLiteral("7f3e2d1c-0b9a-4876-9543-210fedcba987"));

void writeFile(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
}

SnapshotRequest requestFor(const QString &savesDir, const QString &gameId = QStringLiteral("hollow-knight"))
{
    SnapshotRequest request;
    request.gameId = gameId;
    request.machineId = kMachine;
    request.appVersion = QStringLiteral("0.1.0");
    request.profile = {QStringLiteral("official"), QStringLiteral("hollow-knight"), 3};
    for (const char *name : {"user1.dat", "user2.dat"}) {
        request.files.append({savesDir + u'/' + QLatin1String(name),
                              QStringLiteral("<winLocalAppDataLow>/Team Cherry/Hollow Knight/") + QLatin1String(name),
                              QStringLiteral("progress"),
                              {}});
    }
    return request;
}

struct Fixture {
    QTemporaryDir root;
    QString vaultDir() const { return root.path() + QStringLiteral("/vault"); }
    QString savesDir() const { return root.path() + QStringLiteral("/saves"); }

    Fixture()
    {
        writeFile(savesDir() + QStringLiteral("/user1.dat"), "progress of slot 1");
        writeFile(savesDir() + QStringLiteral("/user2.dat"), "progress of slot 2");
    }
};

} // namespace

class TestVault : public QObject {
    Q_OBJECT

private slots:
    void createsAndReopensVault()
    {
        Fixture f;
        const auto created = Vault::open(f.vaultDir());
        QVERIFY2(created.ok(), qPrintable(created.ok() ? QString() : created.error().toString()));
        for (const char *sub : {"vault.json", "blobs/sha256", "snapshots", "tmp", "locks"})
            QVERIFY2(QFileInfo::exists(f.vaultDir() + u'/' + QLatin1String(sub)), sub);

        const auto reopened = Vault::open(f.vaultDir());
        QVERIFY(reopened.ok());
        QCOMPARE(reopened.value().descriptor().vaultId, created.value().descriptor().vaultId);
    }

    void refusesForeignDirectories()
    {
        Fixture f;
        writeFile(f.vaultDir() + QStringLiteral("/my-documents.txt"), "not a vault");
        const auto result = Vault::open(f.vaultDir());
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::InvalidArgument);
    }

    void refusesUnsupportedVaultFormat()
    {
        Fixture f;
        writeFile(f.vaultDir() + QStringLiteral("/vault.json"),
                  R"({"schemaVersion":1,"type":"vault","vaultId":"5b0f6e2a-3c4d-4e5f-8a9b-0c1d2e3f4a5b",
                      "createdAt":"2026-11-01T09:00:00Z","hash":"sha256","compression":"lzma"})");
        QVERIFY(!Vault::open(f.vaultDir()).ok());
    }

    void createsSnapshots()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        QVERIFY(vault.ok());

        const auto snapshot = vault.value().createSnapshot(requestFor(f.savesDir()));
        QVERIFY2(snapshot.ok(), qPrintable(snapshot.ok() ? QString() : snapshot.error().toString()));
        const Snapshot &s = snapshot.value();
        QCOMPARE(s.snapshotId.version(), QUuid::Version(7));
        QCOMPARE(s.files.size(), 2);
        QCOMPARE(s.consistency, Consistency::Consistent);
        QVERIFY(!s.parentId.has_value());
        for (const SnapshotFile &file : s.files)
            QVERIFY(vault.value().blobs().verify(file.sha256).ok());

        // The document on disk is the same snapshot.
        const auto stored = vault.value().snapshot(s.gameId, s.snapshotId);
        QVERIFY(stored.ok());
        QCOMPARE(stored.value().files.first().sha256, s.files.first().sha256);
        QVERIFY(QDir(f.vaultDir() + QStringLiteral("/tmp")).entryList(QDir::Files).isEmpty());
    }

    void deduplicatesAndLinksParents()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        const auto first = vault.value().createSnapshot(requestFor(f.savesDir()));
        writeFile(f.savesDir() + QStringLiteral("/user2.dat"), "slot 2 after playing");
        const auto second = vault.value().createSnapshot(requestFor(f.savesDir()));
        QVERIFY(first.ok() && second.ok());

        QCOMPARE(second.value().parentId, first.value().snapshotId);
        QCOMPARE(second.value().files.at(0).sha256, first.value().files.at(0).sha256); // unchanged file
        QVERIFY(second.value().files.at(1).sha256 != first.value().files.at(1).sha256);
        QCOMPARE(vault.value().blobs().list().size(), 3); // 2 + 1 changed

        const QList<Snapshot> all = vault.value().snapshots(QStringLiteral("hollow-knight"));
        QCOMPARE(all.size(), 2);
        QCOMPARE(all.first().snapshotId, first.value().snapshotId);
        QCOMPARE(vault.value().games(), QStringList{QStringLiteral("hollow-knight")});
    }

    void marksSnapshotsOfChangingFilesInconsistent()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        int reads = 0;
        // Simulates a game writing to user1.dat while it is being backed up.
        const auto snapshot = vault.value().createSnapshot(
            requestFor(f.savesDir()), [&](const QString &path, qsizetype, qsizetype) {
                if (path.endsWith(QLatin1String("user1.dat"))) {
                    ++reads;
                    QFile file(path);
                    if (file.open(QIODevice::Append))
                        file.write("+more");
                }
                return true;
            });
        QVERIFY(snapshot.ok());
        QCOMPARE(reads, 3);
        QCOMPARE(snapshot.value().consistency, Consistency::Inconsistent);
        QCOMPARE(snapshot.value().files.size(), 2);
    }

    void cancellationWritesNoSnapshot()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        const auto result = vault.value().createSnapshot(requestFor(f.savesDir()),
                                                         [](const QString &, qsizetype, qsizetype) { return false; });
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::Cancelled);
        QVERIFY(vault.value().snapshots(QStringLiteral("hollow-knight")).isEmpty());
    }

    void skipsFilesThatVanished()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        QVERIFY(QFile::remove(f.savesDir() + QStringLiteral("/user2.dat")));
        const auto snapshot = vault.value().createSnapshot(requestFor(f.savesDir()));
        QVERIFY(snapshot.ok());
        QCOMPARE(snapshot.value().files.size(), 1);
    }

    void refusesConcurrentWriters()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        QLockFile other(f.vaultDir() + QStringLiteral("/locks/writer.lock"));
        QVERIFY(other.tryLock());
        const auto result = vault.value().createSnapshot(requestFor(f.savesDir()));
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::PermissionDenied);
        other.unlock();
        QVERIFY(vault.value().createSnapshot(requestFor(f.savesDir())).ok());
    }

    void storesLocalGamesUnderSafeDirectoryNames()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        const QString localId = QStringLiteral("local:2f6c1a9e-4b3d-4e2f-9a8b-7c6d5e4f3a2b");
        SnapshotRequest request = requestFor(f.savesDir(), localId);
        request.profile = {QStringLiteral("local"), QStringLiteral("my-indie-game"), 1};
        const auto snapshot = vault.value().createSnapshot(request);
        QVERIFY(snapshot.ok());
        QVERIFY(QDir(f.vaultDir() + QStringLiteral("/snapshots/local_2f6c1a9e-4b3d-4e2f-9a8b-7c6d5e4f3a2b")).exists());
        QCOMPARE(vault.value().games(), QStringList{localId});
        QCOMPARE(vault.value().snapshots(localId).size(), 1);
    }

    void removesUnpinnedSnapshotsOnly()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        const auto a = vault.value().createSnapshot(requestFor(f.savesDir()));
        const auto b = vault.value().createSnapshot(requestFor(f.savesDir()));
        QVERIFY(a.ok() && b.ok());

        QVERIFY(vault.value().removeSnapshot(a.value().gameId, a.value().snapshotId).ok());
        QCOMPARE(vault.value().snapshots(QStringLiteral("hollow-knight")).size(), 1);

        Snapshot pinned = b.value();
        pinned.pinned = true;
        writeFile(vault.value().snapshotPath(pinned.gameId, pinned.snapshotId), toJson(pinned));
        const auto refused = vault.value().removeSnapshot(pinned.gameId, pinned.snapshotId);
        QVERIFY(!refused.ok());
        QCOMPARE(refused.error().code, ErrorCode::PermissionDenied);
    }

    void verifiesHealthyVault()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        QVERIFY(vault.value().createSnapshot(requestFor(f.savesDir())).ok());
        const auto report = vault.value().verify();
        QVERIFY(report.ok());
        QVERIFY(report.value().healthy());
        QCOMPARE(report.value().blobsChecked, 2);
    }

    void verifyReportsCorruptedAndMissingBlobs()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        const auto s = vault.value().createSnapshot(requestFor(f.savesDir()));
        QVERIFY(s.ok());
        const QString corrupted = s.value().files.at(0).sha256;
        const QString missing = s.value().files.at(1).sha256;
        writeFile(vault.value().blobs().blobPath(corrupted), "not zstd");
        QVERIFY(QFile::remove(vault.value().blobs().blobPath(missing)));
        writeFile(vault.value().snapshotPath(QStringLiteral("hollow-knight"), createUuidV7()), "{ damaged");

        const auto report = vault.value().verify();
        QVERIFY(report.ok());
        QVERIFY(!report.value().healthy());
        QCOMPARE(report.value().corruptedBlobs, QStringList{corrupted});
        QCOMPARE(report.value().missingBlobs, QStringList{missing});
        QCOMPARE(report.value().damagedSnapshotFiles.size(), 1);
        QCOMPARE(report.value().affectedSnapshots, (QList<SnapshotRef>{{s.value().gameId, s.value().snapshotId}}));
        // Corrupted blobs are reported, never deleted.
        QVERIFY(QFile::exists(vault.value().blobs().blobPath(corrupted)));
    }

    void garbageCollectionRemovesOnlyOldUnreferencedData()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        const auto a = vault.value().createSnapshot(requestFor(f.savesDir()));
        writeFile(f.savesDir() + QStringLiteral("/user2.dat"), "slot 2 changed");
        const auto b = vault.value().createSnapshot(requestFor(f.savesDir()));
        QVERIFY(a.ok() && b.ok());
        writeFile(f.vaultDir() + QStringLiteral("/tmp/crashed-write.part"), "leftover");

        // Removing the first snapshot orphans the old user2.dat blob.
        QVERIFY(vault.value().removeSnapshot(a.value().gameId, a.value().snapshotId).ok());
        const QString orphan = a.value().files.at(1).sha256;

        // Too recent: protected as a possible write in flight.
        auto report = vault.value().collectGarbage();
        QVERIFY(report.ok());
        QVERIFY(report.value().removedBlobs.isEmpty());
        QCOMPARE(report.value().removedTempFiles, 0);

        // Two hours later the orphan and the leftover temp file go away.
        report = vault.value().collectGarbage(QDateTime::currentDateTimeUtc().addSecs(2 * 3600));
        QVERIFY(report.ok());
        QCOMPARE(report.value().removedBlobs, QStringList{orphan});
        QVERIFY(report.value().freedBytes > 0);
        QCOMPARE(report.value().removedTempFiles, 1);
        QVERIFY(vault.value().verify().value().healthy());
        QCOMPARE(vault.value().blobs().list().size(), 2);
    }

    void garbageCollectionRefusesWhenSnapshotsAreUnreadable()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        QVERIFY(vault.value().createSnapshot(requestFor(f.savesDir())).ok());
        writeFile(vault.value().snapshotPath(QStringLiteral("hollow-knight"), createUuidV7()), "{ damaged");
        const auto report = vault.value().collectGarbage(QDateTime::currentDateTimeUtc().addDays(1));
        QVERIFY(!report.ok());
        QCOMPARE(report.error().code, ErrorCode::Corrupted);
        QCOMPARE(vault.value().blobs().list().size(), 2);
    }

    void skipsDamagedSnapshotFiles()
    {
        Fixture f;
        auto vault = Vault::open(f.vaultDir());
        const auto good = vault.value().createSnapshot(requestFor(f.savesDir()));
        QVERIFY(good.ok());
        writeFile(vault.value().snapshotPath(QStringLiteral("hollow-knight"), createUuidV7()), "{ damaged");
        QCOMPARE(vault.value().snapshots(QStringLiteral("hollow-knight")).size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestVault)
#include "tst_vault.moc"
