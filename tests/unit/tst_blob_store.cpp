#include "saveforge/vault/BlobStore.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTest>

using namespace saveforge;
using namespace saveforge::vault;

namespace {

QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

Result<StoredBlob> storeBytes(BlobStore &store, const QByteArray &data)
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    return store.store(buffer);
}

QByteArray readBack(const BlobStore &store, const QString &sha256, Result<void> *status = nullptr)
{
    QBuffer out;
    out.open(QIODevice::WriteOnly);
    const Result<void> result = store.read(sha256, out);
    if (status)
        *status = result;
    return out.data();
}

QByteArray randomBytes(qsizetype size)
{
    QByteArray data(size, Qt::Uninitialized);
    QRandomGenerator generator(42);
    for (char &c : data)
        c = static_cast<char>(generator.bounded(256));
    return data;
}

} // namespace

class TestBlobStore : public QObject {
    Q_OBJECT

private slots:
    void storesUnderItsHash()
    {
        QTemporaryDir vault;
        BlobStore store(vault.path());
        const QByteArray data("hollow knight save data");

        const auto blob = storeBytes(store, data);
        QVERIFY(blob.ok());
        QCOMPARE(blob.value().sha256, sha256Hex(data));
        QCOMPARE(blob.value().size, data.size());
        QVERIFY(!blob.value().alreadyPresent);

        const QString expected = vault.path() + QStringLiteral("/blobs/sha256/") + blob.value().sha256.left(2) + u'/'
                                 + blob.value().sha256 + QStringLiteral(".zst");
        QCOMPARE(store.blobPath(blob.value().sha256), expected);
        QVERIFY(QFile::exists(expected));
        QVERIFY(store.contains(blob.value().sha256));
        QCOMPARE(store.list(), QStringList{blob.value().sha256});
    }

    void deduplicatesIdenticalContent()
    {
        QTemporaryDir vault;
        BlobStore store(vault.path());
        QVERIFY(storeBytes(store, "same").ok());
        const auto second = storeBytes(store, "same");
        QVERIFY(second.ok());
        QVERIFY(second.value().alreadyPresent);
        QCOMPARE(store.list().size(), 1);
        QVERIFY(QDir(store.tmpDir()).entryList(QDir::Files).isEmpty()); // no leftovers
    }

    void roundTripsLargeAndEmptyContent_data()
    {
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("small") << QByteArray("x");
        QTest::newRow("compressible 4 MiB") << QByteArray(4 * 1024 * 1024, 'a');
        QTest::newRow("random 3 MiB") << randomBytes(3 * 1024 * 1024 + 17);
    }
    void roundTripsLargeAndEmptyContent()
    {
        QFETCH(QByteArray, data);
        QTemporaryDir vault;
        BlobStore store(vault.path());
        const auto blob = storeBytes(store, data);
        QVERIFY(blob.ok());
        Result<void> status;
        QCOMPARE(readBack(store, blob.value().sha256, &status), data);
        QVERIFY(status.ok());
        QVERIFY(store.verify(blob.value().sha256).ok());
    }

    void compressesBlobs()
    {
        QTemporaryDir vault;
        BlobStore store(vault.path());
        const auto blob = storeBytes(store, QByteArray(1024 * 1024, 'z'));
        QVERIFY(blob.ok());
        QVERIFY(QFileInfo(store.blobPath(blob.value().sha256)).size() < 10 * 1024);
    }

    void detectsCorruption()
    {
        QTemporaryDir vault;
        BlobStore store(vault.path());
        const auto blob = storeBytes(store, randomBytes(64 * 1024));
        QVERIFY(blob.ok());
        const QString path = store.blobPath(blob.value().sha256);

        // Flip one byte in the middle of the compressed frame.
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QByteArray bytes = file.readAll();
        bytes[bytes.size() / 2] = static_cast<char>(bytes.at(bytes.size() / 2) ^ 0x01);
        file.seek(0);
        file.write(bytes);
        file.close();

        const auto result = store.verify(blob.value().sha256);
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::Corrupted);
    }

    void detectsTruncation()
    {
        QTemporaryDir vault;
        BlobStore store(vault.path());
        const auto blob = storeBytes(store, randomBytes(64 * 1024));
        QVERIFY(blob.ok());
        QVERIFY(QFile::resize(store.blobPath(blob.value().sha256), 100));
        const auto result = store.verify(blob.value().sha256);
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::Corrupted);
    }

    void detectsContentUnderTheWrongName()
    {
        QTemporaryDir vault;
        BlobStore store(vault.path());
        const auto a = storeBytes(store, "content A");
        const auto b = storeBytes(store, "content B");
        QVERIFY(a.ok() && b.ok());
        QVERIFY(QFile::remove(store.blobPath(a.value().sha256)));
        QVERIFY(QFile::copy(store.blobPath(b.value().sha256), store.blobPath(a.value().sha256)));
        QCOMPARE(store.verify(a.value().sha256).error().code, ErrorCode::Corrupted);
    }

    void reportsMissingAndInvalidHashes()
    {
        QTemporaryDir vault;
        BlobStore store(vault.path());
        QCOMPARE(store.verify(QString(64, u'0')).error().code, ErrorCode::NotFound);
        QCOMPARE(store.verify(QStringLiteral("../../etc/passwd")).error().code, ErrorCode::InvalidArgument);
        QVERIFY(!store.contains(QStringLiteral("../../etc/passwd")));
    }

    void removesBlobs()
    {
        QTemporaryDir vault;
        BlobStore store(vault.path());
        const auto blob = storeBytes(store, "to be removed");
        QVERIFY(blob.ok());
        QVERIFY(store.remove(blob.value().sha256).ok());
        QVERIFY(!store.contains(blob.value().sha256));
        QVERIFY(store.remove(blob.value().sha256).ok()); // idempotent
    }
};

QTEST_GUILESS_MAIN(TestBlobStore)
#include "tst_blob_store.moc"
