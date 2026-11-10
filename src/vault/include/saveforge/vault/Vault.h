#pragma once

#include "saveforge/core/Result.h"
#include "saveforge/vault/BlobStore.h"
#include "saveforge/vault/Snapshot.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QUuid>

#include <chrono>
#include <functional>

namespace saveforge::vault {

// A file to capture, as produced by profile resolution.
struct SnapshotSource {
    QString absolutePath;
    QString templatePath;                  // concrete path in template form
    QString ruleId;
    QHash<QString, QString> bindings;
};

struct SnapshotRequest {
    QString gameId;
    SnapshotKind kind = SnapshotKind::Manual;
    std::optional<SnapshotTrigger> trigger;
    QString label;
    QString slot;                          // save slot whose files are captured (see isValidSlotName)
    QUuid machineId;
    QString appVersion;
    ProfileRef profile;
    QList<SnapshotSource> files;
    QList<RegistryKey> registry;
};

struct SnapshotRef {
    QString gameId;
    QUuid snapshotId;

    bool operator==(const SnapshotRef &) const = default;
};

// Outcome of Vault::verify(). Corrupted blobs are reported, never deleted.
struct VerifyReport {
    qsizetype blobsChecked = 0;
    QStringList corruptedBlobs;           // content does not match its name, or cannot be decoded
    QStringList missingBlobs;             // referenced by a snapshot but absent
    QStringList damagedSnapshotFiles;     // snapshot documents that cannot be read
    QList<SnapshotRef> affectedSnapshots; // snapshots that reference corrupted or missing blobs

    [[nodiscard]] bool healthy() const
    {
        return corruptedBlobs.isEmpty() && missingBlobs.isEmpty() && damagedSnapshotFiles.isEmpty();
    }
};

// Outcome of Vault::collectGarbage().
struct GarbageReport {
    QStringList removedBlobs;
    qint64 freedBytes = 0;                // on-disk (compressed) size of removed blobs
    qsizetype removedTempFiles = 0;
};

// Called after each read attempt of a file; return false to cancel.
// Useful for progress reporting (index counts from 0 to total - 1).
using FileReadCallback = std::function<bool(const QString &absolutePath, qsizetype index, qsizetype total)>;

// A local backup vault (saveforge-specs/vault/SPEC.md).
//
// Layout: vault.json, blobs/, snapshots/<gameDir>/<snapshotId>.json, tmp/, locks/.
// Operations that write take the single-writer lock (locks/writer.lock) and
// fail with ErrorCode::PermissionDenied when another process holds it.
class Vault {
public:
    // Opens an existing vault, or creates one when `directory` is missing or empty.
    // A non-empty directory without vault.json is refused.
    [[nodiscard]] static Result<Vault> open(const QString &directory);

    [[nodiscard]] const QString &path() const { return m_dir; }
    [[nodiscard]] const VaultDescriptor &descriptor() const { return m_descriptor; }
    [[nodiscard]] BlobStore &blobs() { return m_blobs; }
    [[nodiscard]] const BlobStore &blobs() const { return m_blobs; }

    // Captures the request's files and writes an immutable snapshot.
    // Files that disappear before they are read are skipped. If a file keeps
    // changing during three read attempts, the snapshot is marked inconsistent.
    // The snapshot document is written last, after every blob is durable.
    [[nodiscard]] Result<Snapshot> createSnapshot(const SnapshotRequest &request,
                                                  const FileReadCallback &onFileRead = {});

    // Game ids that have at least one snapshot.
    [[nodiscard]] QStringList games() const;
    // Snapshots of a game, oldest first. Unreadable snapshot files are skipped
    // (and reported by verify()).
    [[nodiscard]] QList<Snapshot> snapshots(const QString &gameId) const;
    [[nodiscard]] Result<Snapshot> snapshot(const QString &gameId, const QUuid &snapshotId) const;

    // Deletes a snapshot document. Pinned snapshots are refused. Blobs are
    // reclaimed later by collectGarbage().
    [[nodiscard]] Result<void> removeSnapshot(const QString &gameId, const QUuid &snapshotId);

    [[nodiscard]] QString snapshotPath(const QString &gameId, const QUuid &snapshotId) const;

    // Reads every blob and checks it against its name, and checks that every
    // blob referenced by a snapshot exists (vault spec section 6).
    // `progress(done, total)` may return false to cancel.
    [[nodiscard]] Result<VerifyReport> verify(const std::function<bool(qsizetype, qsizetype)> &progress = {}) const;

    // Mark-and-sweep (vault spec section 5): removes blobs no snapshot references
    // and temporary files, both only when older than `minimumAge`, which protects
    // writes in flight. Refused while any snapshot document is unreadable,
    // because its references would be unknown.
    [[nodiscard]] Result<GarbageReport> collectGarbage(const QDateTime &now = QDateTime::currentDateTimeUtc(),
                                                       std::chrono::seconds minimumAge = std::chrono::hours(1));

private:
    struct Scan {
        QList<Snapshot> snapshots;
        QStringList damagedFiles;
    };

    Vault(QString directory, VaultDescriptor descriptor);
    [[nodiscard]] Scan scanAllSnapshots() const;

    QString m_dir;
    VaultDescriptor m_descriptor;
    BlobStore m_blobs;
};

} // namespace saveforge::vault
