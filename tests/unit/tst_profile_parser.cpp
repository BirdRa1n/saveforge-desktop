// Conformance test: every example in savevault-specs/profile/examples/profile.v1
// must be accepted (valid/) or rejected (invalid/) exactly as the schema says.

#include "savevault/profiles/ProfileParser.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using namespace savevault;
using namespace savevault::profiles;

namespace {

const QString kExamples = QStringLiteral(SAVEVAULT_SPECS_DIR "/profile/examples/profile.v1");

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("cannot open %s", qPrintable(path));
    return file.readAll();
}

QJsonObject example(const QString &name)
{
    return QJsonDocument::fromJson(readFile(kExamples + QStringLiteral("/valid/") + name)).object();
}

void addExampleRows(const QString &kind)
{
    QTest::addColumn<QString>("file");
    const QStringList files = QDir(kExamples + u'/' + kind).entryList({QStringLiteral("*.json")}, QDir::Files);
    QVERIFY2(!files.isEmpty(), "no examples found; is the specs submodule initialized?");
    for (const QString &file : files)
        QTest::newRow(qPrintable(file)) << kExamples + u'/' + kind + u'/' + file;
}

} // namespace

class TestProfileParser : public QObject {
    Q_OBJECT

private slots:
    void acceptsValidExamples_data() { addExampleRows(QStringLiteral("valid")); }
    void acceptsValidExamples()
    {
        QFETCH(QString, file);
        const auto result = parseProfile(readFile(file));
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.error().toString()));
    }

    void rejectsInvalidExamples_data() { addExampleRows(QStringLiteral("invalid")); }
    void rejectsInvalidExamples()
    {
        QFETCH(QString, file);
        const auto result = parseProfile(readFile(file));
        QVERIFY2(!result.ok(), "invalid example was accepted");
        QCOMPARE(result.error().code, ErrorCode::InvalidArgument);
    }

    void readsAllFields()
    {
        const auto result = parseProfile(example(QStringLiteral("hollow-knight.json")));
        QVERIFY(result.ok());
        const Profile &p = result.value();
        QCOMPARE(p.id, QStringLiteral("hollow-knight"));
        QCOMPARE(p.revision, 3);
        QCOMPARE(p.identifiers.steam, QList<qint64>{367520});
        QCOMPARE(p.saves.size(), 3);
        QCOMPARE(p.saves.first().when.os, platform::Os::Windows);
        QCOMPARE(p.saves.first().tags, QList<Tag>{Tag::Save});
        QCOMPARE(p.registry.first().keys.first(), QStringLiteral("HKCU/Software/Team Cherry/Hollow Knight"));
        QCOMPARE(p.backup.cloudSync, QStringList{QStringLiteral("steam")});
        QCOMPARE(p.mechanism.type, QStringLiteral("standard"));
        QCOMPARE(p.minAppVersion, QStringLiteral("0.1.0"));
        QCOMPARE(p.provenance.license, QStringLiteral("CC-BY-SA-4.0"));
    }

    // Profiles published before the rename name their source "saveforge".
    void acceptsTheFormerSourceName()
    {
        QJsonObject json = example(QStringLiteral("hollow-knight.json"));
        QJsonObject provenance = json.value(QStringLiteral("provenance")).toObject();
        provenance.insert(QStringLiteral("source"), QStringLiteral("saveforge"));
        json.insert(QStringLiteral("provenance"), provenance);
        const auto result = parseProfile(json);
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.error().message));
        QCOMPARE(result.value().provenance.source, QStringLiteral("savevault"));
    }

    void unknownMechanismIsUnsupported()
    {
        QJsonObject json = example(QStringLiteral("hollow-knight.json"));
        json.insert(QStringLiteral("mechanism"), QJsonObject{{QStringLiteral("type"), QStringLiteral("future-thing")}});
        const auto result = parseProfile(json);
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::Unsupported);
        QCOMPARE(result.error().context, QStringLiteral("/mechanism/type"));
    }

    void newerSchemaVersionIsUnsupported()
    {
        QJsonObject json = example(QStringLiteral("hollow-knight.json"));
        json.insert(QStringLiteral("schemaVersion"), 2);
        const auto result = parseProfile(json);
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::Unsupported);
    }

    void errorsPointAtTheOffendingValue()
    {
        QJsonObject json = example(QStringLiteral("hollow-knight.json"));
        QJsonArray saves = json.value(QStringLiteral("saves")).toArray();
        QJsonObject rule = saves.at(1).toObject();
        rule.insert(QStringLiteral("paths"), QJsonArray{QStringLiteral("<xdgConfig>/../.ssh/id_rsa")});
        saves.replace(1, rule);
        json.insert(QStringLiteral("saves"), saves);

        const auto result = parseProfile(json);
        QVERIFY(!result.ok());
        QCOMPARE(result.error().context, QStringLiteral("/saves/1/paths/0"));
        QVERIFY(result.error().message.contains(QStringLiteral("P4")));
    }

    void rejectsNonIntegerNumbers()
    {
        QJsonObject json = example(QStringLiteral("hollow-knight.json"));
        json.insert(QStringLiteral("revision"), 1.5);
        const auto result = parseProfile(json);
        QVERIFY(!result.ok());
        QCOMPARE(result.error().context, QStringLiteral("/revision"));
    }

    void rejectsMalformedJson()
    {
        QVERIFY(!parseProfile(QByteArray("{ not json")).ok());
        QVERIFY(!parseProfile(QByteArray("[]")).ok());
    }
};

QTEST_GUILESS_MAIN(TestProfileParser)
#include "tst_profile_parser.moc"
