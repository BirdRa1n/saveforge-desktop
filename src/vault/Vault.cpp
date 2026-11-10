#include "savevault/vault/Vault.h"

#include "savevault/core/Uuid.h"
#include "savevault/platform/FileSync.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QLoggingCategory>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <memory>

Q_LOGGING_CATEGORY(lcVault, "savevault.vault")

namespace savevault::vault {

namespace {

constexpr int kMaxReadAttempts = 3;
constexpr int kLockTimeoutMs = 500;

Error ioError(const QString &message, const QString &context)
{
    return Error{ErrorCode::Io, message, context};
}

// Writes `data` to tmp/, fsyncs it and renames it to `target`.
Result<void> writeDurably(const QString &tmpDir, const QString &target, const QByteArray &data)
{
    if (!QDir().mkpath(tmpDir) || !QDir().mkpath(QFileInfo(target).absolutePath()))
        return ioError(QStringLiteral("cannot create directory"), target);

    QFile tmp(tmpDir + u'/' + uuidToString(createUuidV7()) + QStringLiteral(".json.part"));
    if (!tmp.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return ioError(QStringLiteral("cannot create temporary file"), tmp.fileName());
    if (tmp.write(data) != data.size() || !platform::syncFile(tmp)) {
        tmp.close();
        tmp.remove();
        return ioError(QStringLiteral("cannot write file"), target);
    }
    tmp.close();
    QFile::remove(target); // only used for rewrites of the same document
    if (!tmp.rename(target)) {
        tmp.remove();
        return ioError(QStringLiteral("cannot move file into place"), target);
    }
    platform::syncDirectory(QFileInfo(target).absolutePath());
    return {};
}

// Holds the vault's single-writer lock for the lifetime of the object.
class WriterLock {
public:
    explicit WriterLock(const QString &vaultDir)
        : m_lock(vaultDir + QStringLiteral("/locks/writer.lock"))
    {
        QDir().mkpath(vaultDir + QStringLiteral("/locks"));
        m_lock.setStaleLockTime(0); // stale only when the owning process is gone
    }

    [[nodiscard]] Result<void> acquire()
    {
        if (m_lock.tryLock(kLockTimeoutMs))
            return {};
        qint64 pid = 0;
        QString host;
        QString app;
        m_lock.getLockInfo(&pid, &host, &app);
        return Error{ErrorCode::PermissionDenied, QStringLiteral("the vault is being written by another process"),
                     QStringLiteral("pid %1 on %2").arg(pid).arg(host)};
    }

private:
    QLockFile m_lock;
};

struct FileState {
    bool exists = false;
    qint64 size = -1;
    QDateTime mtime;

    bool operator==(const FileState &) const = default;
};

FileState stat(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return {};
    return {true, info.size(), info.lastModified(QTimeZone::UTC)};
}

} // namespace

Vault::Vault(QString directory, VaultDescriptor descriptor)
    : m_dir(std::move(directory))
    , m_descriptor(std::move(descriptor))
    , m_blobs(m_dir)
{
}

Result<Vault> Vault::open(const QString &directory)
{
    const QString dir = QDir::cleanPath(QDir(directory).absolutePath());
    const QString descriptorPath = dir + QStringLiteral("/vault.json");

    if (QFileInfo::exists(descriptorPath)) {
        QFile file(descriptorPath);
        if (!file.open(QIODevice::ReadOnly))
            return ioError(QStringLiteral("cannot read vault.json: ") + file.errorString(), descriptorPath);
        auto descriptor = parseVaultDescriptor(file.readAll());
        if (!descriptor.ok())
            return descriptor.error();
        return Vault(dir, std::move(descriptor).value());
    }

    const QDir existing(dir);
    if (existing.exists() && !existing.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden))
        return Error{ErrorCode::InvalidArgument, QStringLiteral("directory is not empty and is not a SaveVault vault"), dir};

    for (const char *sub : {"blobs/sha256", "snapshots", "tmp", "locks"}) {
        if (!QDir().mkpath(dir + u'/' + QLatin1String(sub)))
            return ioError(QStringLiteral("cannot create vault directory"), dir + u'/' + QLatin1String(sub));
    }
    VaultDescriptor descriptor{createUuidV7(), QDateTime::currentDateTimeUtc()};
    if (auto written = writeDurably(dir + QStringLiteral("/tmp"), descriptorPath, toJson(descriptor)); !written.ok())
        return written.error();
    qCInfo(lcVault) << "created vault" << dir;
    return Vault(dir, descriptor);
}

QString Vault::snapshotPath(const QString &gameId, const QUuid &snapshotId) const
{
    return m_dir + QStringLiteral("/snapshots/") + gameDirectoryName(gameId) + u'/' + uuidToString(snapshotId)
           + QStringLiteral(".json");
}

Result<Snapshot> Vault::createSnapshot(const SnapshotRequest &request, const FileReadCallback &onFileRead)
{
    WriterLock lock(m_dir);
    if (auto acquired = lock.acquire(); !acquired.ok())
        return acquired.error();

    Snapshot snapshot;
    snapshot.snapshotId = createUuidV7();
    snapshot.gameId = request.gameId;
    snapshot.kind = request.kind;
    snapshot.trigger = request.trigger;
    snapshot.label = request.label;
    snapshot.slot = request.slot;
    snapshot.machineId = request.machineId;
    snapshot.appVersion = request.appVersion;
    snapshot.profile = request.profile;
    snapshot.registry = request.registry;

    // The parent is the newest snapshot of this game taken on this machine.
    const QList<Snapshot> previous = snapshots(request.gameId);
    for (auto it = previous.crbegin(); it != previous.crend(); ++it) {
        if (it->machineId == request.machineId) {
            snapshot.parentId = it->snapshotId;
            break;
        }
    }

    const qsizetype total = request.files.size();
    // Per request file: its position in snapshot.files (-1 when it vanished)
    // and its state right after it was read.
    QList<qsizetype> position(total, -1);
    QList<FileState> readState(total);
    // Reads one file into the blob store, re-reading it while it changes.
    const auto capture = [&](qsizetype index) -> Result<void> {
        const SnapshotSource &source = request.files.at(index);
        bool stable = false;
        FileState after;
        StoredBlob blob;
        for (int attempt = 0; attempt < kMaxReadAttempts && !stable; ++attempt) {
            const FileState before = stat(source.absolutePath);
            if (!before.exists)
                break; // vanished since profile resolution: nothing to capture
            auto stored = m_blobs.storeFile(source.absolutePath);
            if (!stored.ok()) {
                if (!QFileInfo::exists(source.absolutePath))
                    break;
                return stored.error();
            }
            blob = std::move(stored).value();
            if (onFileRead && !onFileRead(source.absolutePath, index, total))
                return Error{ErrorCode::Cancelled, QStringLiteral("backup cancelled"), source.absolutePath};
            after = stat(source.absolutePath);
            stable = after == before && after.size == blob.size;
        }
        if (blob.sha256.isEmpty())
            return {};
        if (!stable) {
            snapshot.consistency = Consistency::Inconsistent;
            qCWarning(lcVault) << "file kept changing while being read:" << source.absolutePath;
        }
        const SnapshotFile file{source.templatePath, source.bindings, source.ruleId, blob.sha256, blob.size,
                                after.exists ? after.mtime : QDateTime::currentDateTimeUtc()};
        if (position.at(index) < 0) {
            position[index] = snapshot.files.size();
            snapshot.files.append(file);
        } else {
            snapshot.files[position.at(index)] = file;
        }
        readState[index] = after;
        return {};
    };
    for (qsizetype index = 0; index < total; ++index) {
        if (auto captured = capture(index); !captured.ok())
            return captured.error();
    }

    // Files that must be read together: when a member changed after it was
    // read (e.g. a database written while its journal was copied), read the
    // whole group again.
    for (const QList<qsizetype> &group : request.consistentGroups) {
        bool consistent = false;
        for (int attempt = 0; attempt < kMaxReadAttempts && !consistent; ++attempt) {
            consistent = std::all_of(group.cbegin(), group.cend(), [&](qsizetype index) {
                return index < 0 || index >= total || position.at(index) < 0
                       || stat(request.files.at(index).absolutePath) == readState.at(index);
            });
            if (consistent)
                break;
            for (qsizetype index : group) {
                if (index >= 0 && index < total) {
                    if (auto captured = capture(index); !captured.ok())
                        return captured.error();
                }
            }
        }
        if (!consistent) {
            snapshot.consistency = Consistency::Inconsistent;
            qCWarning(lcVault) << "a group of files kept changing while being read";
        }
    }

    snapshot.createdAt = QDateTime::currentDateTimeUtc();
    // Validate before writing: a document this vault cannot read back must never be stored.
    const QByteArray json = toJson(snapshot);
    if (auto check = parseSnapshot(json); !check.ok())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("snapshot is invalid: ") + check.error().message,
                     check.error().context};
    if (auto written = writeDurably(m_blobs.tmpDir(), snapshotPath(snapshot.gameId, snapshot.snapshotId), json);
        !written.ok())
        return written.error();
    return snapshot;
}

Result<bool> Vault::importSnapshot(const Snapshot &snapshot, const BlobSource &source)
{
    WriterLock lock(m_dir);
    if (auto acquired = lock.acquire(); !acquired.ok())
        return acquired.error();
    if (QFile::exists(snapshotPath(snapshot.gameId, snapshot.snapshotId)))
        return false;

    const QByteArray json = toJson(snapshot);
    if (auto check = parseSnapshot(json); !check.ok())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("snapshot is invalid: ") + check.error().message,
                     check.error().context};

    for (const SnapshotFile &file : snapshot.files) {
        if (m_blobs.contains(file.sha256))
            continue;
        auto bytes = source ? source(file.sha256)
                            : Result<QByteArray>(Error{ErrorCode::NotFound, QStringLiteral("blob missing"), file.sha256});
        if (!bytes.ok())
            return bytes.error();
        QBuffer buffer(&bytes.value());
        if (!buffer.open(QIODevice::ReadOnly))
            return ioError(QStringLiteral("cannot read blob data"), file.sha256);
        auto stored = m_blobs.store(buffer);
        if (!stored.ok())
            return stored.error();
        if (stored.value().sha256 != file.sha256)
            return Error{ErrorCode::Corrupted, QStringLiteral("blob content does not match its hash"), file.sha256};
    }
    if (auto written = writeDurably(m_blobs.tmpDir(), snapshotPath(snapshot.gameId, snapshot.snapshotId), json);
        !written.ok())
        return written.error();
    qCInfo(lcVault) << "imported snapshot" << uuidToString(snapshot.snapshotId) << "of" << snapshot.gameId;
    return true;
}

QStringList Vault::games() const
{
    QStringList ids;
    const QDir dir(m_dir + QStringLiteral("/snapshots"));
    for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QString id = name;
        if (id.startsWith(QLatin1String("local_")))
            id[5] = u':';
        if (!QDir(dir.filePath(name)).entryList({QStringLiteral("*.json")}, QDir::Files).isEmpty())
            ids.append(id);
    }
    return ids;
}

QList<Snapshot> Vault::snapshots(const QString &gameId) const
{
    QList<Snapshot> result;
    const QDir dir(m_dir + QStringLiteral("/snapshots/") + gameDirectoryName(gameId));
    for (const QFileInfo &info : dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        auto parsed = parseSnapshot(file.readAll());
        if (!parsed.ok() || parsed.value().gameId != gameId
            || uuidToString(parsed.value().snapshotId) + QStringLiteral(".json") != info.fileName()) {
            qCWarning(lcVault) << "skipping unreadable snapshot" << info.absoluteFilePath();
            continue;
        }
        result.append(std::move(parsed).value());
    }
    std::sort(result.begin(), result.end(), [](const Snapshot &a, const Snapshot &b) {
        return a.createdAt != b.createdAt ? a.createdAt < b.createdAt
                                          : uuidToString(a.snapshotId) < uuidToString(b.snapshotId);
    });
    return result;
}

Result<Snapshot> Vault::snapshot(const QString &gameId, const QUuid &snapshotId) const
{
    QFile file(snapshotPath(gameId, snapshotId));
    if (!file.exists())
        return Error{ErrorCode::NotFound, QStringLiteral("snapshot not found"), uuidToString(snapshotId)};
    if (!file.open(QIODevice::ReadOnly))
        return ioError(QStringLiteral("cannot read snapshot: ") + file.errorString(), file.fileName());
    auto parsed = parseSnapshot(file.readAll());
    if (!parsed.ok())
        return Error{ErrorCode::Corrupted, QStringLiteral("snapshot file is damaged: ") + parsed.error().message,
                     file.fileName()};
    return parsed;
}

Vault::Scan Vault::scanAllSnapshots() const
{
    Scan scan;
    const QDir root(m_dir + QStringLiteral("/snapshots"));
    for (const QString &gameDir : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        for (const QFileInfo &info : QDir(root.filePath(gameDir)).entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
            QFile file(info.absoluteFilePath());
            auto parsed = file.open(QIODevice::ReadOnly) ? parseSnapshot(file.readAll())
                                                         : Result<Snapshot>(Error{ErrorCode::Io, file.errorString(), {}});
            if (!parsed.ok() || gameDirectoryName(parsed.value().gameId) != gameDir) {
                scan.damagedFiles.append(info.absoluteFilePath());
                continue;
            }
            scan.snapshots.append(std::move(parsed).value());
        }
    }
    return scan;
}

Result<VerifyReport> Vault::verify(const std::function<bool(qsizetype, qsizetype)> &progress) const
{
    VerifyReport report;
    const Scan scan = scanAllSnapshots();
    report.damagedSnapshotFiles = scan.damagedFiles;

    QHash<QString, QList<SnapshotRef>> referencedBy;
    for (const Snapshot &s : scan.snapshots) {
        for (const SnapshotFile &file : s.files) {
            QList<SnapshotRef> &refs = referencedBy[file.sha256];
            const SnapshotRef ref{s.gameId, s.snapshotId};
            if (!refs.contains(ref))
                refs.append(ref);
        }
    }

    const QStringList blobs = m_blobs.list();
    for (qsizetype i = 0; i < blobs.size(); ++i) {
        const auto result = m_blobs.verify(blobs.at(i));
        ++report.blobsChecked;
        if (!result.ok() && result.error().code == ErrorCode::Corrupted)
            report.corruptedBlobs.append(blobs.at(i));
        else if (!result.ok() && result.error().code != ErrorCode::NotFound)
            return result.error();
        if (progress && !progress(i + 1, blobs.size()))
            return Error{ErrorCode::Cancelled, QStringLiteral("verification cancelled"), {}};
    }

    const QSet<QString> present(blobs.cbegin(), blobs.cend());
    for (auto it = referencedBy.cbegin(); it != referencedBy.cend(); ++it) {
        if (!present.contains(it.key()))
            report.missingBlobs.append(it.key());
    }
    report.missingBlobs.sort();

    for (const QString &hash : report.corruptedBlobs + report.missingBlobs) {
        for (const SnapshotRef &ref : referencedBy.value(hash)) {
            if (!report.affectedSnapshots.contains(ref))
                report.affectedSnapshots.append(ref);
        }
    }
    if (!report.healthy())
        qCWarning(lcVault) << "vault verification found problems:" << report.corruptedBlobs.size() << "corrupted,"
                           << report.missingBlobs.size() << "missing," << report.damagedSnapshotFiles.size()
                           << "damaged snapshot files";
    return report;
}

Result<GarbageReport> Vault::collectGarbage(const QDateTime &now, std::chrono::seconds minimumAge)
{
    WriterLock lock(m_dir);
    if (auto acquired = lock.acquire(); !acquired.ok())
        return acquired.error();

    const Scan scan = scanAllSnapshots();
    if (!scan.damagedFiles.isEmpty())
        return Error{ErrorCode::Corrupted,
                     QStringLiteral("refusing to collect garbage while snapshot files are unreadable"),
                     scan.damagedFiles.join(QLatin1String(", "))};

    QSet<QString> referenced;
    for (const Snapshot &s : scan.snapshots) {
        for (const SnapshotFile &file : s.files)
            referenced.insert(file.sha256);
    }

    const QDateTime cutoff = now.addSecs(-minimumAge.count());
    GarbageReport report;
    for (const QString &hash : m_blobs.list()) {
        if (referenced.contains(hash) || m_blobs.lastModified(hash) > cutoff)
            continue;
        const qint64 size = QFileInfo(m_blobs.blobPath(hash)).size();
        if (auto removed = m_blobs.remove(hash); !removed.ok())
            return removed.error();
        report.removedBlobs.append(hash);
        report.freedBytes += size;
    }

    for (const QFileInfo &info : QDir(m_blobs.tmpDir()).entryInfoList(QDir::Files | QDir::Hidden)) {
        if (info.lastModified(QTimeZone::UTC) <= cutoff && QFile::remove(info.absoluteFilePath()))
            ++report.removedTempFiles;
    }
    qCInfo(lcVault) << "garbage collection removed" << report.removedBlobs.size() << "blobs," << report.freedBytes
                    << "bytes," << report.removedTempFiles << "temporary files";
    return report;
}

Result<Snapshot> Vault::setPinned(const QString &gameId, const QUuid &snapshotId, bool pinned)
{
    WriterLock lock(m_dir);
    if (auto acquired = lock.acquire(); !acquired.ok())
        return acquired.error();
    auto existing = snapshot(gameId, snapshotId);
    if (!existing.ok())
        return existing.error();
    Snapshot updated = std::move(existing).value();
    if (updated.pinned == pinned)
        return updated;
    updated.pinned = pinned;
    if (auto written = writeDurably(m_blobs.tmpDir(), snapshotPath(gameId, snapshotId), toJson(updated)); !written.ok())
        return written.error();
    return updated;
}

Result<void> Vault::removeSnapshot(const QString &gameId, const QUuid &snapshotId)
{
    WriterLock lock(m_dir);
    if (auto acquired = lock.acquire(); !acquired.ok())
        return acquired.error();

    auto existing = snapshot(gameId, snapshotId);
    if (!existing.ok())
        return existing.error();
    if (existing.value().pinned)
        return Error{ErrorCode::PermissionDenied, QStringLiteral("pinned snapshots cannot be removed"),
                     uuidToString(snapshotId)};
    if (!QFile::remove(snapshotPath(gameId, snapshotId)))
        return ioError(QStringLiteral("cannot remove snapshot"), snapshotPath(gameId, snapshotId));
    return {};
}

} // namespace savevault::vault
