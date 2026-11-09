#pragma once

#include "saveforge/core/Result.h"

#include <QDateTime>
#include <QIODevice>
#include <QString>
#include <QStringList>

namespace saveforge::vault {

// Result of storing one content.
struct StoredBlob {
    QString sha256;          // of the original bytes; also the blob's name
    qint64 size = 0;         // original size
    bool alreadyPresent = false;
};

// Content-addressed blob storage inside a vault
// (saveforge-specs/vault/SPEC.md sections 1 and 2):
//   blobs/sha256/<first two hex>/<sha256>.zst  — one zstd frame of the original bytes
//
// Writes go to tmp/, are fsync'ed and renamed into place, so a crash never
// leaves a partially written blob under blobs/. Reads verify the SHA-256 and
// report ErrorCode::Corrupted on any mismatch or decoding error.
//
// BlobStore does not lock: callers that write must hold the vault's writer lock.
class BlobStore {
public:
    explicit BlobStore(const QString &vaultDir);

    [[nodiscard]] QString blobPath(const QString &sha256) const;
    [[nodiscard]] bool contains(const QString &sha256) const;

    // Hashes and compresses `source` from its current position to the end.
    [[nodiscard]] Result<StoredBlob> store(QIODevice &source);
    [[nodiscard]] Result<StoredBlob> storeFile(const QString &path);

    // Decompresses a blob into `destination`, verifying its hash.
    [[nodiscard]] Result<void> read(const QString &sha256, QIODevice &destination) const;
    // Reads and verifies a blob without keeping its content.
    [[nodiscard]] Result<void> verify(const QString &sha256) const;

    // Hashes of every blob present.
    [[nodiscard]] QStringList list() const;
    [[nodiscard]] QDateTime lastModified(const QString &sha256) const;
    [[nodiscard]] Result<void> remove(const QString &sha256);

    [[nodiscard]] QString tmpDir() const { return m_tmpDir; }

private:
    QString m_blobsDir;
    QString m_tmpDir;
};

} // namespace saveforge::vault
