#include "saveforge/vault/BlobStore.h"

#include "saveforge/core/Uuid.h"
#include "saveforge/platform/FileSync.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimeZone>

#include <zstd.h>

#include <memory>
#include <vector>

namespace saveforge::vault {

namespace {

constexpr int kCompressionLevel = 3;

struct CCtxDeleter { void operator()(ZSTD_CCtx *ctx) const { ZSTD_freeCCtx(ctx); } };
struct DCtxDeleter { void operator()(ZSTD_DCtx *ctx) const { ZSTD_freeDCtx(ctx); } };

const QRegularExpression &hashPattern()
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{64}$"));
    return re;
}

Error ioError(const QString &message, const QString &context)
{
    return Error{ErrorCode::Io, message, context};
}

Error corrupted(const QString &sha256, const QString &reason)
{
    return Error{ErrorCode::Corrupted, QStringLiteral("blob is corrupted: ") + reason, sha256};
}

// Writes `size` bytes or reports failure.
bool writeAll(QIODevice &device, const char *data, qsizetype size)
{
    return device.write(data, size) == size;
}

} // namespace

BlobStore::BlobStore(const QString &vaultDir)
    : m_blobsDir(vaultDir + QStringLiteral("/blobs/sha256"))
    , m_tmpDir(vaultDir + QStringLiteral("/tmp"))
{
}

QString BlobStore::blobPath(const QString &sha256) const
{
    return m_blobsDir + u'/' + sha256.left(2) + u'/' + sha256 + QStringLiteral(".zst");
}

bool BlobStore::contains(const QString &sha256) const
{
    return hashPattern().match(sha256).hasMatch() && QFileInfo::exists(blobPath(sha256));
}

Result<StoredBlob> BlobStore::store(QIODevice &source)
{
    if (!QDir().mkpath(m_tmpDir))
        return ioError(QStringLiteral("cannot create tmp directory"), m_tmpDir);

    QFile tmp(m_tmpDir + u'/' + uuidToString(createUuidV7()) + QStringLiteral(".part"));
    if (!tmp.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return ioError(QStringLiteral("cannot create temporary file"), tmp.fileName());
    auto discard = [&tmp](Error error) -> Result<StoredBlob> {
        tmp.close();
        tmp.remove();
        return error;
    };

    std::unique_ptr<ZSTD_CCtx, CCtxDeleter> cctx(ZSTD_createCCtx());
    ZSTD_CCtx_setParameter(cctx.get(), ZSTD_c_compressionLevel, kCompressionLevel);
    ZSTD_CCtx_setParameter(cctx.get(), ZSTD_c_checksumFlag, 1);

    std::vector<char> in(ZSTD_CStreamInSize());
    std::vector<char> out(ZSTD_CStreamOutSize());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;

    // Feeds `input` to the compressor; with ZSTD_e_end, also flushes the frame.
    auto compress = [&](ZSTD_inBuffer input, ZSTD_EndDirective mode) -> bool {
        for (;;) {
            ZSTD_outBuffer output{out.data(), out.size(), 0};
            const size_t remaining = ZSTD_compressStream2(cctx.get(), &output, &input, mode);
            if (ZSTD_isError(remaining))
                return false;
            if (!writeAll(tmp, out.data(), static_cast<qsizetype>(output.pos)))
                return false;
            const bool done = mode == ZSTD_e_end ? remaining == 0 : input.pos == input.size;
            if (done)
                return true;
        }
    };

    for (;;) {
        const qint64 read = source.read(in.data(), static_cast<qint64>(in.size()));
        if (read < 0)
            return discard(ioError(QStringLiteral("cannot read source"), source.errorString()));
        if (read == 0)
            break;
        hash.addData(QByteArrayView(in.data(), static_cast<qsizetype>(read)));
        total += read;
        if (!compress(ZSTD_inBuffer{in.data(), static_cast<size_t>(read), 0}, ZSTD_e_continue))
            return discard(ioError(QStringLiteral("cannot compress blob"), tmp.fileName()));
    }
    if (!compress(ZSTD_inBuffer{nullptr, 0, 0}, ZSTD_e_end))
        return discard(ioError(QStringLiteral("cannot compress blob"), tmp.fileName()));
    if (!platform::syncFile(tmp))
        return discard(ioError(QStringLiteral("cannot persist blob"), tmp.fileName()));
    tmp.close();

    StoredBlob blob{QString::fromLatin1(hash.result().toHex()), total, false};
    const QString target = blobPath(blob.sha256);
    if (QFileInfo::exists(target)) {
        tmp.remove();
        blob.alreadyPresent = true;
        return blob;
    }
    const QString targetDir = QFileInfo(target).absolutePath();
    if (!QDir().mkpath(targetDir))
        return discard(ioError(QStringLiteral("cannot create blob directory"), targetDir));
    if (!tmp.rename(target)) {
        // Another writer may have stored the same content in the meantime.
        if (QFileInfo::exists(target)) {
            tmp.remove();
            blob.alreadyPresent = true;
            return blob;
        }
        return discard(ioError(QStringLiteral("cannot move blob into place"), target));
    }
    platform::syncDirectory(targetDir);
    return blob;
}

Result<StoredBlob> BlobStore::storeFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return ioError(QStringLiteral("cannot open file: ") + file.errorString(), path);
    return store(file);
}

Result<void> BlobStore::read(const QString &sha256, QIODevice &destination) const
{
    if (!hashPattern().match(sha256).hasMatch())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("not a SHA-256 hash"), sha256};
    QFile file(blobPath(sha256));
    if (!file.exists())
        return Error{ErrorCode::NotFound, QStringLiteral("blob not found"), sha256};
    if (!file.open(QIODevice::ReadOnly))
        return ioError(QStringLiteral("cannot open blob: ") + file.errorString(), file.fileName());

    std::unique_ptr<ZSTD_DCtx, DCtxDeleter> dctx(ZSTD_createDCtx());
    std::vector<char> in(ZSTD_DStreamInSize());
    std::vector<char> out(ZSTD_DStreamOutSize());
    QCryptographicHash hash(QCryptographicHash::Sha256);
    size_t lastResult = 1; // non-zero: frame not finished

    for (;;) {
        const qint64 read = file.read(in.data(), static_cast<qint64>(in.size()));
        if (read < 0)
            return ioError(QStringLiteral("cannot read blob"), file.fileName());
        if (read == 0)
            break;
        ZSTD_inBuffer input{in.data(), static_cast<size_t>(read), 0};
        while (input.pos < input.size) {
            ZSTD_outBuffer output{out.data(), out.size(), 0};
            lastResult = ZSTD_decompressStream(dctx.get(), &output, &input);
            if (ZSTD_isError(lastResult))
                return corrupted(sha256, QString::fromLatin1(ZSTD_getErrorName(lastResult)));
            hash.addData(QByteArrayView(out.data(), static_cast<qsizetype>(output.pos)));
            if (!writeAll(destination, out.data(), static_cast<qsizetype>(output.pos)))
                return ioError(QStringLiteral("cannot write decompressed data"), destination.errorString());
        }
    }
    if (lastResult != 0)
        return corrupted(sha256, QStringLiteral("truncated frame"));
    if (QString::fromLatin1(hash.result().toHex()) != sha256)
        return corrupted(sha256, QStringLiteral("content does not match its hash"));
    return {};
}

Result<void> BlobStore::verify(const QString &sha256) const
{
    // Discards output; only the hash check matters.
    class NullDevice final : public QIODevice {
    public:
        NullDevice() { open(QIODevice::WriteOnly); }
    protected:
        qint64 readData(char *, qint64) override { return -1; }
        qint64 writeData(const char *, qint64 size) override { return size; }
    } sink;
    return read(sha256, sink);
}

QStringList BlobStore::list() const
{
    QStringList hashes;
    QDirIterator it(m_blobsDir, {QStringLiteral("*.zst")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString name = it.nextFileInfo().completeBaseName();
        if (hashPattern().match(name).hasMatch())
            hashes.append(name);
    }
    hashes.sort();
    return hashes;
}

QDateTime BlobStore::lastModified(const QString &sha256) const
{
    return QFileInfo(blobPath(sha256)).lastModified(QTimeZone::UTC);
}

Result<void> BlobStore::remove(const QString &sha256)
{
    QFile file(blobPath(sha256));
    if (file.exists() && !file.remove())
        return ioError(QStringLiteral("cannot remove blob: ") + file.errorString(), file.fileName());
    return {};
}

} // namespace saveforge::vault
