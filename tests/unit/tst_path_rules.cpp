// Conformance test: the C++ path rules must agree with every vector in
// savevault-specs/profile/test-vectors/path-rules.json.

#include "savevault/profiles/PathRules.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace savevault::profiles;
using savevault::platform::Os;

namespace {

QJsonObject loadVectors()
{
    QFile file(QStringLiteral(SAVEVAULT_SPECS_DIR "/profile/test-vectors/path-rules.json"));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("cannot open path-rules.json; is the specs submodule initialized?");
    return QJsonDocument::fromJson(file.readAll()).object();
}

void addRows(const char *group)
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<QString>("rule");
    for (const QJsonValue &value : loadVectors().value(QLatin1String(group)).toArray()) {
        const QJsonObject c = value.toObject();
        const QString input = c.value(QLatin1String("input")).toString();
        QTest::newRow(qPrintable(input.isEmpty() ? QStringLiteral("<empty>") : input))
            << input << c.value(QLatin1String("valid")).toBool() << c.value(QLatin1String("rule")).toString();
    }
}

void verify(const RuleCheck &check, bool valid, const QString &rule)
{
    QCOMPARE(check.valid(), valid);
    if (!valid)
        QCOMPARE(check.violations.first(), rule);
}

} // namespace

class TestPathRules : public QObject {
    Q_OBJECT

private slots:
    void savePaths_data() { addRows("savePaths"); }
    void savePaths()
    {
        QFETCH(QString, input);
        QFETCH(bool, valid);
        QFETCH(QString, rule);
        verify(PathRules::builtin().checkSavePath(input), valid, rule);
    }

    void registryKeys_data() { addRows("registryKeys"); }
    void registryKeys()
    {
        QFETCH(QString, input);
        QFETCH(bool, valid);
        QFETCH(QString, rule);
        verify(PathRules::builtin().checkRegistryKey(input), valid, rule);
    }

    void vectorsAreNotEmpty()
    {
        const QJsonObject vectors = loadVectors();
        QVERIFY(vectors.value(QLatin1String("savePaths")).toArray().size() > 20);
        QVERIFY(vectors.value(QLatin1String("registryKeys")).toArray().size() > 5);
    }

    void impliedOs()
    {
        const PathRules &rules = PathRules::builtin();
        QCOMPARE(rules.impliedOs(u"<winAppData>/Studio/Game"), Os::Windows);
        QCOMPARE(rules.impliedOs(u"<xdgConfig>/Studio/Game"), Os::Linux);
        QCOMPARE(rules.impliedOs(u"<macAppSupport>/Studio/Game"), Os::MacOS);
        QVERIFY(!rules.impliedOs(u"<home>/Studio/Game").has_value());
        QVERIFY(!rules.impliedOs(u"<base>/saves").has_value());
        QVERIFY(!rules.impliedOs(u"C:/Games").has_value());
    }

    void runtimeSensitiveCheckOnConcreteSegments()
    {
        const PathRules &rules = PathRules::builtin();
        QVERIFY(rules.isSensitive({QStringLiteral("Microsoft"), QStringLiteral("Credentials"), QStringLiteral("x")}));
        QVERIFY(rules.isSensitive({QStringLiteral("Games"), QStringLiteral(".SSH")}));
        QVERIFY(!rules.isSensitive({QStringLiteral("Team Cherry"), QStringLiteral("Hollow Knight"), QStringLiteral("user1.dat")}));
    }
};

QTEST_GUILESS_MAIN(TestPathRules)
#include "tst_path_rules.moc"
