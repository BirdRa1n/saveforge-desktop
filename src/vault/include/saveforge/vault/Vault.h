#pragma once

#include "saveforge/core/Result.h"
#include "saveforge/vault/BlobStore.h"
#include "saveforge/vault/Snapshot.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QUuid>

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
    QUuid machineId;
    QString appVersion;
    ProfileRef profile;
    QList<SnapshotSource> files;
    QList<RegistryKey> registry;
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

private:
    Vault(QString directory, VaultDescriptor descriptor);

    QString m_dir;
    VaultDescriptor m_descriptor;
    BlobStore m_blobs;
};

} // namespace saveforge::vault
