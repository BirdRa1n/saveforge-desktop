// Conformance test: snapshot and vault documents against the examples in
// saveforge-specs/vault/examples, plus serialization round-trips.

#include "saveforge/vault/Snapshot.h"

#include <QDir>
#include <QFile>
#include <QTest>

using namespace saveforge;
using namespace saveforge::vault;

namespace {

const QString kExamples = QStringLiteral(SAVEFORGE_SPECS_DIR "/vault/examples");

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("cannot open %s", qPrintable(path));
    return file.readAll();
}

void addRows(const QString &schema, const QString &kind)
{
    QTest::addColumn<QString>("file");
    const QString dir = kExamples + u'/' + schema + u'/' + kind;
    const QStringList files = QDir(dir).entryList({QStringLiteral("*.json")}, QDir::Files);
    QVERIFY2(!files.isEmpty(), qPrintable(QStringLiteral("no examples in ") + dir));
    for (const QString &file : files)
        QTest::newRow(qPrintable(file)) << dir + u'/' + file;
}

} // namespace

class TestSnapshotJson : public QObject {
    Q_OBJECT

private slots:
    void acceptsValidSnapshots_data() { addRows(QStringLiteral("snapshot.v1"), QStringLiteral("valid")); }
    void acceptsValidSnapshots()
    {
        QFETCH(QString, file);
        const auto result = parseSnapshot(readFile(file));
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.error().toString()));
    }

    void rejectsInvalidSnapshots_data() { addRows(QStringLiteral("snapshot.v1"), QStringLiteral("invalid")); }
    void rejectsInvalidSnapshots()
    {
        QFETCH(QString, file);
        QVERIFY2(!parseSnapshot(readFile(file)).ok(), "invalid example was accepted");
    }

    void acceptsValidVaultDescriptors_data() { addRows(QStringLiteral("vault.v1"), QStringLiteral("valid")); }
    void acceptsValidVaultDescriptors()
    {
        QFETCH(QString, file);
        QVERIFY(parseVaultDescriptor(readFile(file)).ok());
    }

    void rejectsInvalidVaultDescriptors_data() { addRows(QStringLiteral("vault.v1"), QStringLiteral("invalid")); }
    void rejectsInvalidVaultDescriptors()
    {
        QFETCH(QString, file);
        QVERIFY(!parseVaultDescriptor(readFile(file)).ok());
    }

    void roundTripsEveryField()
    {
        const auto parsed = parseSnapshot(readFile(kExamples + QStringLiteral("/snapshot.v1/valid/manual-windows.json")));
        QVERIFY(parsed.ok());
        const Snapshot &original = parsed.value();
        QCOMPARE(original.kind, SnapshotKind::Manual);
        QVERIFY(original.parentId.has_value());
        QCOMPARE(original.registry.first().values.size(), 4);

        const auto again = parseSnapshot(toJson(original));
        QVERIFY2(again.ok(), qPrintable(again.ok() ? QString() : again.error().toString()));
        const Snapshot &copy = again.value();
        QCOMPARE(copy.snapshotId, original.snapshotId);
        QCOMPARE(copy.parentId, original.parentId);
        QCOMPARE(copy.createdAt, original.createdAt);
        QCOMPARE(copy.label, original.label);
        QCOMPARE(copy.profile.revision, original.profile.revision);
        QCOMPARE(copy.files.size(), original.files.size());
        QCOMPARE(copy.files.first().sha256, original.files.first().sha256);
        QCOMPARE(copy.files.first().mtime, original.files.first().mtime);
        QCOMPARE(copy.registry.first().values.at(1).data, original.registry.first().values.at(1).data);
    }

    void roundTripsBindingsAndTrigger()
    {
        const auto parsed = parseSnapshot(readFile(kExamples + QStringLiteral("/snapshot.v1/valid/auto-store-user-scoped.json")));
        QVERIFY(parsed.ok());
        QCOMPARE(parsed.value().trigger, SnapshotTrigger::GameExit);
        const auto again = parseSnapshot(toJson(parsed.value()));
        QVERIFY(again.ok());
        QCOMPARE(again.value().files.first().bindings.value(QStringLiteral("storeUserId")),
                 QStringLiteral("76561198000000000"));
        QCOMPARE(again.value().consistency, Consistency::Inconsistent);
    }

    void serializesWithLfAndUtcTimestamps()
    {
        Snapshot s;
        s.snapshotId = QUuid::fromString(QStringLiteral("0193f1c0-5a2b-7c3d-8e4f-a1b2c3d4e5f6"));
        s.gameId = QStringLiteral("hollow-knight");
        s.createdAt = QDateTime::fromString(QStringLiteral("2026-11-08T19:10:00-03:00"), Qt::ISODate);
        s.machineId = QUuid::fromString(QStringLiteral("7f3e2d1c-0b9a-4876-9543-210fedcba987"));
        s.appVersion = QStringLiteral("0.1.0");
        s.profile = {QStringLiteral("official"), QStringLiteral("hollow-knight"), 3};

        const QByteArray json = toJson(s);
        QVERIFY(!json.contains('\r'));
        QVERIFY(json.contains("\"createdAt\": \"2026-11-08T22:10:00.000Z\""));
        QVERIFY(json.contains("\"parentId\": null"));
        QVERIFY(parseSnapshot(json).ok());
    }

    void gameDirectoryNames()
    {
        QCOMPARE(gameDirectoryName(QStringLiteral("hollow-knight")), QStringLiteral("hollow-knight"));
        QCOMPARE(gameDirectoryName(QStringLiteral("local:2f6c1a9e-4b3d-4e2f-9a8b-7c6d5e4f3a2b")),
                 QStringLiteral("local_2f6c1a9e-4b3d-4e2f-9a8b-7c6d5e4f3a2b"));
    }
};

QTEST_GUILESS_MAIN(TestSnapshotJson)
#include "tst_snapshot_json.moc"
