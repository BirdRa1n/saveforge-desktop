#include "saveforge/archive/Gamesave.h"
#include "saveforge/core/Uuid.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <miniz.h>

#include <cstring>

using namespace saveforge;
using namespace saveforge::archive;

namespace {

const QString kExamples = QStringLiteral(SAVEFORGE_SPECS_DIR "/gamesave/examples/manifest.v1");

QString sha(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

vault::Snapshot snapshotWith(const QString &gameId, const QList<QByteArray> &contents)
{
    vault::Snapshot s;
    s.snapshotId = createUuidV7();
    s.gameId = gameId;
    s.createdAt = QDateTime::currentDateTimeUtc();
    s.machineId = createUuidV7();
    s.appVersion = QStringLiteral("0.1.0");
    s.profile = {QStringLiteral("upstream-ludusavi"), gameId, 1};
    int n = 0;
    for (const QByteArray &content : contents) {
        vault::SnapshotFile file;
        file.path = QStringLiteral("<winAppData>/Game/slot%1.sav").arg(++n);
        file.ruleId = QStringLiteral("save");
        file.sha256 = sha(content);
        file.size = content.size();
        file.mtime = s.createdAt;
        s.files.append(file);
    }
    return s;
}

// Builds a ZIP by hand, for archives a well-behaved writer never produces.
struct Entry {
    QString name;
    QByteArray bytes;
    bool deflate = false;
};

QString writeZip(const QString &path, const QList<Entry> &entries)
{
    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 0))
        return {};
    for (const Entry &e : entries)
        mz_zip_writer_add_mem(&zip, e.name.toUtf8().constData(), e.bytes.constData(), static_cast<size_t>(e.bytes.size()),
                              e.deflate ? MZ_DEFAULT_LEVEL : MZ_NO_COMPRESSION);
    void *buffer = nullptr;
    size_t size = 0;
    mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    QFile out(path);
    if (out.open(QIODevice::WriteOnly))
        out.write(static_cast<const char *>(buffer), static_cast<qint64>(size));
    mz_zip_writer_end(&zip);
    return path;
}

} // namespace

class TestGamesave : public QObject {
    Q_OBJECT

    // A valid manifest and its blob, to build variations from.
    static QPair<QByteArray, QByteArray> validParts()
    {
        const QByteArray content("progress");
        GamesaveManifest m;
        m.exportId = createUuidV7();
        m.createdAt = QDateTime::currentDateTimeUtc();
        m.appVersion = QStringLiteral("0.1.0");
        m.sourceOs = QStringLiteral("windows");
        m.games.append({QStringLiteral("hollow-knight"), QStringLiteral("Hollow Knight"), std::nullopt,
                        {snapshotWith(QStringLiteral("hollow-knight"), {content})}});
        return {toJson(m), content};
    }

    static QList<Entry> validEntries()
    {
        const auto [manifest, content] = validParts();
        return {{QStringLiteral("mimetype"), QByteArray(kGamesaveMediaType)},
                {QStringLiteral("manifest.json"), manifest, true},
                {QStringLiteral("manifest.json.sha256"), sha(manifest).toLatin1()},
                {QStringLiteral("blobs/") + sha(content), content, true}};
    }

private slots:
    void acceptsValidManifests_data()
    {
        QTest::addColumn<QString>("file");
        for (const QString &name : QDir(kExamples + QStringLiteral("/valid")).entryList({QStringLiteral("*.json")}))
            QTest::newRow(qPrintable(name)) << kExamples + QStringLiteral("/valid/") + name;
    }
    void acceptsValidManifests()
    {
        QFETCH(QString, file);
        const auto parsed = parseManifest(readAll(file));
        QVERIFY2(parsed.ok(), qPrintable(parsed.ok() ? QString() : parsed.error().toString()));
        // Writing it back gives a manifest that parses to the same content.
        const auto again = parseManifest(toJson(parsed.value()));
        QVERIFY(again.ok());
        QCOMPARE(again.value().games.size(), parsed.value().games.size());
        QCOMPARE(again.value().referencedBlobs(), parsed.value().referencedBlobs());
    }

    void rejectsInvalidManifests_data()
    {
        QTest::addColumn<QString>("file");
        for (const QString &name : QDir(kExamples + QStringLiteral("/invalid")).entryList({QStringLiteral("*.json")}))
            QTest::newRow(qPrintable(name)) << kExamples + QStringLiteral("/invalid/") + name;
    }
    void rejectsInvalidManifests()
    {
        QFETCH(QString, file);
        QVERIFY(!parseManifest(readAll(file)).ok());
    }

    void roundTripsAnExport()
    {
        QTemporaryDir dir;
        const QByteArray a("slot one"), b(QByteArray(4096, 'x')); // b compresses well
        GamesaveManifest m;
        m.exportId = createUuidV7();
        m.createdAt = QDateTime::currentDateTimeUtc();
        m.appVersion = QStringLiteral("0.1.0");
        m.sourceOs = QStringLiteral("linux");
        m.placeholders.insert(QStringLiteral("<home>"), QStringLiteral("/home/player"));
        m.games.append({QStringLiteral("game-a"), QStringLiteral("Game A"), std::nullopt,
                        {snapshotWith(QStringLiteral("game-a"), {a, b})}});
        const QHash<QString, QByteArray> store{{sha(a), a}, {sha(b), b}};
        const BlobProvider provider = [&store](const QString &hash) -> Result<QByteArray> {
            if (!store.contains(hash))
                return Error{ErrorCode::NotFound, QStringLiteral("missing"), hash};
            return store.value(hash);
        };

        const QString path = dir.path() + QStringLiteral("/export.gamesave");
        QVERIFY(writeGamesave(path, m, provider).ok());
        QVERIFY(QDir(dir.path()).entryList({QStringLiteral("*.tmp")}, QDir::Files | QDir::Hidden).isEmpty());

        const auto reader = GamesaveReader::open(path);
        QVERIFY2(reader.ok(), qPrintable(reader.ok() ? QString() : reader.error().toString()));
        QCOMPARE(reader.value().manifest().exportId, m.exportId);
        QCOMPARE(reader.value().manifest().placeholders.value(QStringLiteral("<home>")), QStringLiteral("/home/player"));
        QCOMPARE(reader.value().manifest().games.first().snapshots.first().snapshotId,
                 m.games.first().snapshots.first().snapshotId);
        QCOMPARE(reader.value().blob(sha(b)).value(), b);
        QVERIFY(reader.value().missingBlobs().isEmpty());
    }

    void incrementalExportsOmitTheBaseBlobs()
    {
        QTemporaryDir dir;
        const QByteArray a("old"), b("new");
        GamesaveManifest m;
        m.exportId = createUuidV7();
        m.createdAt = QDateTime::currentDateTimeUtc();
        m.appVersion = QStringLiteral("0.1.0");
        m.sourceOs = QStringLiteral("windows");
        m.baseExportId = createUuidV7();
        m.games.append({QStringLiteral("game-a"), QStringLiteral("Game A"), std::nullopt,
                        {snapshotWith(QStringLiteral("game-a"), {a, b})}});
        const BlobProvider provider = [&](const QString &hash) -> Result<QByteArray> { return hash == sha(a) ? a : b; };
        const QString path = dir.path() + QStringLiteral("/inc.gamesave");
        QVERIFY(writeGamesave(path, m, provider, {sha(a)}).ok());
        const auto reader = GamesaveReader::open(path);
        QVERIFY(reader.ok());
        QCOMPARE(reader.value().missingBlobs(), QSet<QString>{sha(a)});

        // A full export that lacks a blob does not verify, and is not left behind.
        m.baseExportId.reset();
        const QString full = dir.path() + QStringLiteral("/full.gamesave");
        QVERIFY(!writeGamesave(full, m, provider, {sha(a)}).ok());
        QVERIFY(!QFile::exists(full));
    }

    void acceptsAHandBuiltValidArchive()
    {
        QTemporaryDir dir;
        QVERIFY(GamesaveReader::open(writeZip(dir.path() + QStringLiteral("/ok.gamesave"), validEntries())).ok());
    }

    void rejectsDangerousArchives_data()
    {
        QTest::addColumn<QList<Entry>>("entries");
        const QList<Entry> valid = validEntries();

        QList<Entry> mimetypeLast = valid;
        mimetypeLast.move(0, mimetypeLast.size() - 1);
        QTest::newRow("mimetype not first") << mimetypeLast;

        QList<Entry> mimetypeDeflated = valid;
        mimetypeDeflated[0].bytes = QByteArray(kGamesaveMediaType).repeated(1);
        mimetypeDeflated[0].deflate = true;
        QTest::newRow("mimetype compressed") << mimetypeDeflated;

        QList<Entry> traversal = valid;
        traversal.append({QStringLiteral("../evil.txt"), "x"});
        QTest::newRow("path traversal") << traversal;

        QList<Entry> backslash = valid;
        backslash.append({QStringLiteral("blobs\\x"), "x"});
        QTest::newRow("backslash") << backslash;

        QList<Entry> extra = valid;
        extra.append({QStringLiteral("notes.txt"), "x"});
        QTest::newRow("unexpected entry") << extra;

        QList<Entry> wrongHash = valid;
        wrongHash[2].bytes = QByteArray(64, '0');
        QTest::newRow("manifest hash mismatch") << wrongHash;

        QList<Entry> badBlob = valid;
        badBlob[3].bytes = "tampered";
        QTest::newRow("blob does not match its name") << badBlob;

        QList<Entry> missingBlob = valid;
        missingBlob.removeLast();
        QTest::newRow("full export missing a blob") << missingBlob;

        QList<Entry> bomb = valid;
        bomb.append({QStringLiteral("README.txt"), QByteArray(8 * 1024 * 1024, '\0'), true});
        QTest::newRow("compression ratio above 1000:1") << bomb;

        QList<Entry> duplicate = valid;
        duplicate.append({QStringLiteral("MANIFEST.JSON"), "x"});
        QTest::newRow("names equal ignoring case") << duplicate;
    }
    void rejectsDangerousArchives()
    {
        QFETCH(QList<Entry>, entries);
        QTemporaryDir dir;
        const auto reader = GamesaveReader::open(writeZip(dir.path() + QStringLiteral("/bad.gamesave"), entries));
        QVERIFY(!reader.ok());
    }

    void enforcesTheSizeLimit()
    {
        QTemporaryDir dir;
        GamesaveLimits limits;
        limits.maxTotalBytes = 100;
        const auto reader = GamesaveReader::open(writeZip(dir.path() + QStringLiteral("/big.gamesave"), validEntries()), limits);
        QVERIFY(!reader.ok());
    }

    void refusesNewerFormats()
    {
        QByteArray manifest = validParts().first;
        manifest.replace("\"formatVersion\": 1", "\"formatVersion\": 2");
        const auto parsed = parseManifest(manifest);
        QVERIFY(!parsed.ok());
        QCOMPARE(parsed.error().code, ErrorCode::Unsupported);
    }
};

Q_DECLARE_METATYPE(Entry)

QTEST_GUILESS_MAIN(TestGamesave)
#include "tst_gamesave.moc"
