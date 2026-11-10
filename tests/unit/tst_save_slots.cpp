#include "ServiceFixture.h"

#include "savevault/services/BackupService.h"
#include "savevault/services/RestoreService.h"
#include "savevault/services/SaveSlots.h"

using namespace savevault;
using namespace savevault::services;
using namespace savevault::testing;

namespace {

vault::Snapshot backup(ServiceFixture &f, const QString &slot)
{
    BackupOptions options;
    options.slot = slot;
    auto snapshot = BackupService(*f.vault, f.environment()).backup(f.target(), options);
    if (!snapshot.ok())
        qFatal("backup failed: %s", qPrintable(snapshot.error().toString()));
    return snapshot.value();
}

} // namespace

class TestSaveSlots : public QObject {
    Q_OBJECT

private slots:
    void backupsRecordTheSlot()
    {
        ServiceFixture f;
        writeFile(f.saveDir() + QStringLiteral("/save.sav"), "progress");
        QCOMPARE(backup(f, QStringLiteral("Personagem A")).slot, QStringLiteral("Personagem A"));
        QVERIFY(backup(f, {}).slot.isEmpty());
        // The slot survives a round trip through the vault.
        QCOMPARE(f.vault->snapshots(f.target().effectiveGameId()).first().slot, QStringLiteral("Personagem A"));
    }

    void switchingKeepsTheProgressOfBothSlots()
    {
        ServiceFixture f;
        const QString save = f.saveDir() + QStringLiteral("/save.sav");

        // Character A, then "save as" character B, then more play with B.
        writeFile(save, "A: chapter 2");
        const vault::Snapshot a = backup(f, QStringLiteral("Personagem A"));
        writeFile(save, "B: chapter 1");
        backup(f, QStringLiteral("Personagem B"));
        writeFile(save, "B: chapter 4");
        backup(f, {}); // unnamed progress is not a slot

        // Switch to A: restore A's head; the files on disk belong to B.
        QList<SaveSlot> found = saveSlots(*f.vault, f.target().effectiveGameId());
        QCOMPARE(found.size(), 2);
        const auto headA = std::find_if(found.cbegin(), found.cend(), [](const SaveSlot &s) { return s.name == u"Personagem A"; });
        QVERIFY(headA != found.cend());
        QCOMPARE(headA->head.snapshotId, a.snapshotId);

        RestoreService restore(*f.vault, f.environment());
        const auto plan = restore.plan(f.target(), headA->head.snapshotId);
        QVERIFY(plan.ok());
        const auto report = restore.execute(f.target(), plan.value(), CloudPolicy::Ask, QStringLiteral("Personagem B"));
        QVERIFY2(report.ok(), qPrintable(report.ok() ? QString() : report.error().toString()));
        QCOMPARE(readFile(save), QByteArray("A: chapter 2"));

        // B's current state is the progress it had when the user switched away.
        found = saveSlots(*f.vault, f.target().effectiveGameId());
        QCOMPARE(found.first().name, QStringLiteral("Personagem B")); // most recently updated
        QCOMPARE(found.first().head.kind, vault::SnapshotKind::PreRestore);
        QCOMPARE(found.first().snapshots, 2);
        QCOMPARE(found.first().head.snapshotId, *report.value().preRestoreSnapshotId);

        // Switching back brings B's latest progress.
        const auto back = restore.plan(f.target(), found.first().head.snapshotId);
        QVERIFY(back.ok());
        QVERIFY(restore.execute(f.target(), back.value(), CloudPolicy::Ask, QStringLiteral("Personagem A")).ok());
        QCOMPARE(readFile(save), QByteArray("B: chapter 4"));
    }

    void gamesWithoutSlotsHaveNone()
    {
        ServiceFixture f;
        writeFile(f.saveDir() + QStringLiteral("/save.sav"), "progress");
        backup(f, {});
        QVERIFY(saveSlots(*f.vault, f.target().effectiveGameId()).isEmpty());
        QVERIFY(saveSlots(*f.vault, QStringLiteral("unknown-game")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSaveSlots)
#include "tst_save_slots.moc"
