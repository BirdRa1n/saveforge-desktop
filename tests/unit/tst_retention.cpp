#include "ServiceFixture.h"

#include "savevault/core/Uuid.h"

#include <QTimeZone>
#include "savevault/services/BackupService.h"
#include "savevault/services/Retention.h"

using namespace savevault;
using namespace savevault::services;
using namespace savevault::testing;

namespace {

const QDateTime kNow = QDateTime(QDate(2026, 11, 10), QTime(12, 0), QTimeZone::UTC);

vault::Snapshot snap(int daysAgo, vault::SnapshotKind kind = vault::SnapshotKind::Manual, const QString &slot = {},
                     bool pinned = false)
{
    vault::Snapshot s;
    s.snapshotId = createUuidV7();
    s.gameId = QStringLiteral("game");
    s.createdAt = kNow.addDays(-daysAgo);
    s.kind = kind;
    s.slot = slot;
    s.pinned = pinned;
    return s;
}

QList<int> ages(const QList<vault::Snapshot> &snapshots)
{
    QList<int> days;
    for (const vault::Snapshot &s : snapshots)
        days.append(static_cast<int>(s.createdAt.daysTo(kNow)));
    return days;
}

} // namespace

class TestRetention : public QObject {
    Q_OBJECT

private slots:
    void keepsEverythingByDefault()
    {
        QVERIFY(snapshotsToRemove({snap(30), snap(20), snap(1)}, {}, kNow).isEmpty());
    }

    void keepsTheNewestN()
    {
        const QList<vault::Snapshot> all{snap(50), snap(40), snap(30), snap(20), snap(10)};
        QCOMPARE(ages(snapshotsToRemove(all, {2, 0}, kNow)), (QList<int>{50, 40, 30}));
    }

    void keepsRecentDays()
    {
        const QList<vault::Snapshot> all{snap(50), snap(40), snap(5), snap(1)};
        QCOMPARE(ages(snapshotsToRemove(all, {0, 30}, kNow)), (QList<int>{50, 40}));
        // Either limit keeps a snapshot.
        QCOMPARE(ages(snapshotsToRemove(all, {3, 30}, kNow)), (QList<int>{50}));
    }

    void neverRemovesProtectedSnapshots()
    {
        const QList<vault::Snapshot> all{
            snap(90, vault::SnapshotKind::Manual, {}, true),            // pinned
            snap(80, vault::SnapshotKind::Manual, QStringLiteral("A")), // head of slot A
            snap(70, vault::SnapshotKind::PreRestore),                  // older undo point
            snap(60, vault::SnapshotKind::PreRestore),                  // newest undo point
            snap(50, vault::SnapshotKind::Manual, QStringLiteral("B")),
            snap(40, vault::SnapshotKind::Manual, QStringLiteral("B")), // head of slot B
            snap(30)};                                                  // newest overall
        QCOMPARE(ages(snapshotsToRemove(all, {1, 0}, kNow)), (QList<int>{70, 50}));
    }

    void appliesToAVaultAndFreesSpace()
    {
        ServiceFixture f;
        BackupService service(*f.vault, f.environment());
        for (int i = 0; i < 4; ++i) {
            writeFile(f.saveDir() + QStringLiteral("/slot.sav"), QByteArray("version ") + QByteArray::number(i));
            QVERIFY(service.backup(f.target()).ok());
        }
        const QString gameId = f.target().effectiveGameId();
        // Blobs younger than an hour are kept for writes in flight: run "later".
        const QDateTime later = QDateTime::currentDateTimeUtc().addSecs(2 * 3600);
        const auto report = applyRetention(*f.vault, {2, 0}, {}, later);
        QVERIFY2(report.ok(), qPrintable(report.ok() ? QString() : report.error().toString()));
        QCOMPARE(report.value().removedSnapshots, 2);
        QVERIFY(report.value().freedBytes > 0);
        QCOMPARE(f.vault->snapshots(gameId).size(), 2);
        QVERIFY(f.vault->verify().value().healthy());
    }

    void pinnedSnapshotsSurvive()
    {
        ServiceFixture f;
        BackupService service(*f.vault, f.environment());
        writeFile(f.saveDir() + QStringLiteral("/slot.sav"), "keep me");
        const auto first = service.backup(f.target());
        QVERIFY(first.ok());
        writeFile(f.saveDir() + QStringLiteral("/slot.sav"), "newer");
        QVERIFY(service.backup(f.target()).ok());
        const QString gameId = f.target().effectiveGameId();
        QVERIFY(f.vault->setPinned(gameId, first.value().snapshotId, true).value().pinned);
        QVERIFY(f.vault->snapshot(gameId, first.value().snapshotId).value().pinned);

        QCOMPARE(applyRetention(*f.vault, {1, 0}).value().removedSnapshots, 0);
        QCOMPARE(f.vault->snapshots(gameId).size(), 2);

        QVERIFY(!f.vault->setPinned(gameId, first.value().snapshotId, false).value().pinned);
        QCOMPARE(applyRetention(*f.vault, {1, 0}).value().removedSnapshots, 1);
    }
};

QTEST_GUILESS_MAIN(TestRetention)
#include "tst_retention.moc"
