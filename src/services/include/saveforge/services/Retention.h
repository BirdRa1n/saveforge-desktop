#pragma once

#include "saveforge/core/Result.h"
#include "saveforge/vault/Vault.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace saveforge::services {

// Which snapshots of each game to keep (roadmap D2.3). Zero means "no limit";
// with both zero nothing is removed.
struct RetentionPolicy {
    int keepLast = 0;     // keep at least the newest N snapshots of each game
    int keepDays = 0;     // keep every snapshot younger than N days

    [[nodiscard]] bool keepsEverything() const { return keepLast <= 0 && keepDays <= 0; }
};

// Snapshots of one game that the policy would remove, oldest first. Never
// removed, whatever the policy (vault/SPEC.md section 5 and save slots):
//   - pinned snapshots;
//   - the newest pre-restore snapshot, the undo point of the last restore;
//   - the newest snapshot of each save slot, which is that slot's progress;
//   - the newest snapshot of the game.
// With both limits set, a snapshot is kept when either keeps it.
[[nodiscard]] QList<vault::Snapshot> snapshotsToRemove(const QList<vault::Snapshot> &snapshots,
                                                       const RetentionPolicy &policy,
                                                       const QDateTime &now = QDateTime::currentDateTimeUtc());

struct RetentionReport {
    qsizetype removedSnapshots = 0;
    qint64 freedBytes = 0;        // on-disk size of blobs no snapshot needs any more
    QStringList failures;
};

// Applies the policy to the given games (every game in the vault when empty)
// and, with `collectGarbage`, removes the blobs nothing references any more
// (a full scan of the vault; after a single backup it can wait).
[[nodiscard]] Result<RetentionReport> applyRetention(vault::Vault &vault, const RetentionPolicy &policy,
                                                     const QStringList &gameIds = {},
                                                     const QDateTime &now = QDateTime::currentDateTimeUtc(),
                                                     bool collectGarbage = true);

} // namespace saveforge::services
