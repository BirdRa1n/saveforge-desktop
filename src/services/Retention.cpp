#include "saveforge/services/Retention.h"

#include "saveforge/core/Uuid.h"

#include <QHash>
#include <QLoggingCategory>
#include <QSet>

#include <chrono>

Q_LOGGING_CATEGORY(lcRetention, "saveforge.retention")

namespace saveforge::services {

QList<vault::Snapshot> snapshotsToRemove(const QList<vault::Snapshot> &snapshots, const RetentionPolicy &policy,
                                         const QDateTime &now)
{
    if (policy.keepsEverything() || snapshots.isEmpty())
        return {};

    // Newest first, with the same tie-break as the vault (creation time, then id).
    QList<vault::Snapshot> ordered = snapshots;
    std::sort(ordered.begin(), ordered.end(), [](const vault::Snapshot &a, const vault::Snapshot &b) {
        return a.createdAt != b.createdAt ? a.createdAt > b.createdAt
                                          : uuidToString(a.snapshotId) > uuidToString(b.snapshotId);
    });

    QSet<QUuid> protectedIds{ordered.first().snapshotId};
    QSet<QString> slotsSeen;
    bool preRestoreSeen = false;
    for (const vault::Snapshot &snapshot : std::as_const(ordered)) {
        if (snapshot.pinned)
            protectedIds.insert(snapshot.snapshotId);
        if (snapshot.kind == vault::SnapshotKind::PreRestore && !preRestoreSeen) {
            protectedIds.insert(snapshot.snapshotId);
            preRestoreSeen = true;
        }
        if (!snapshot.slot.isEmpty() && !slotsSeen.contains(snapshot.slot)) {
            protectedIds.insert(snapshot.snapshotId);
            slotsSeen.insert(snapshot.slot);
        }
    }

    const QDateTime cutoff = policy.keepDays > 0 ? now.addDays(-policy.keepDays) : QDateTime();
    QList<vault::Snapshot> removed;
    for (qsizetype i = 0; i < ordered.size(); ++i) {
        const vault::Snapshot &snapshot = ordered.at(i);
        if (protectedIds.contains(snapshot.snapshotId))
            continue;
        const bool keptByCount = policy.keepLast > 0 && i < policy.keepLast;
        const bool keptByAge = cutoff.isValid() && snapshot.createdAt >= cutoff;
        if (!keptByCount && !keptByAge)
            removed.prepend(snapshot);
    }
    return removed;
}

Result<RetentionReport> applyRetention(vault::Vault &vault, const RetentionPolicy &policy, const QStringList &gameIds,
                                       const QDateTime &now, bool collectGarbage)
{
    RetentionReport report;
    if (policy.keepsEverything())
        return report;
    const QStringList games = gameIds.isEmpty() ? vault.games() : gameIds;
    for (const QString &gameId : games) {
        for (const vault::Snapshot &snapshot : snapshotsToRemove(vault.snapshots(gameId), policy, now)) {
            if (auto removed = vault.removeSnapshot(gameId, snapshot.snapshotId); removed.ok())
                ++report.removedSnapshots;
            else
                report.failures.append(gameId + QStringLiteral(": ") + removed.error().message);
        }
    }
    if (report.removedSnapshots > 0 && collectGarbage) {
        // The vault keeps blobs younger than an hour to protect writes in
        // flight; anything older that no snapshot needs goes now.
        auto garbage = vault.collectGarbage(now);
        if (!garbage.ok())
            return garbage.error();
        report.freedBytes = garbage.value().freedBytes;
    }
    qCInfo(lcRetention) << "removed" << report.removedSnapshots << "snapshot(s), freed" << report.freedBytes << "bytes";
    return report;
}

} // namespace saveforge::services
