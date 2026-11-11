#include "savevault/registry/JsonSpans.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using namespace savevault::registry;

namespace {

QByteArray slice(const QByteArray &text, JsonSpan span)
{
    return text.mid(span.begin, span.end - span.begin);
}

} // namespace

class TestJsonSpans : public QObject {
    Q_OBJECT

private slots:
    void splitsAnObjectIntoMembers()
    {
        const QByteArray text = " { \"a\": 1, \"b\" : \"x\", \"c\": [1, {\"d\": 2}], \"e\": {\"f\": null} } ";
        const JsonSpans spans(text);
        const auto root = spans.root();
        QVERIFY(root);
        const auto members = spans.objectMembers(*root);
        QVERIFY(members);
        QCOMPARE(members->size(), 4);
        QCOMPARE(slice(text, members->value(QStringLiteral("a"))), QByteArray("1"));
        QCOMPARE(slice(text, members->value(QStringLiteral("b"))), QByteArray("\"x\""));
        QCOMPARE(slice(text, members->value(QStringLiteral("c"))), QByteArray("[1, {\"d\": 2}]"));
        QCOMPARE(spans.parse(members->value(QStringLiteral("e")))->toObject().value(QStringLiteral("f")).isNull(), true);
    }

    void listsArrayElements()
    {
        const QByteArray text = "[{\"id\": \"a\"}, {\"id\": \"b\"} ,true, -1.5e3, \"s\"]";
        const JsonSpans spans(text);
        const auto elements = spans.arrayElements(*spans.root());
        QVERIFY(elements);
        QCOMPARE(elements->size(), 5);
        QCOMPARE(spans.parse(elements->at(1))->toObject().value(QStringLiteral("id")).toString(), QStringLiteral("b"));
        QCOMPARE(spans.parse(elements->at(2))->toBool(), true);
        QCOMPARE(spans.parse(elements->at(3))->toDouble(), -1500.0);
        QCOMPARE(spans.parse(elements->at(4))->toString(), QStringLiteral("s"));
        QVERIFY(spans.arrayElements(JsonSpan{0, 2}) == std::nullopt); // "[{" is not an array
        const QByteArray empty = "[ ]";
        QCOMPARE(JsonSpans(empty).arrayElements(*JsonSpans(empty).root())->size(), 0);
    }

    // Brackets, quotes and escapes inside strings must not confuse the scanner.
    void respectsStringsAndEscapes()
    {
        const QByteArray text = "{\"k\": \"a } ] [ { \\\" \\\\\", \"path\": \"<base>/x\\\\\\\"y\", \"n\": [\"]\", \"\\\\\"]}";
        const JsonSpans spans(text);
        const auto members = spans.objectMembers(*spans.root());
        QVERIFY(members);
        QCOMPARE(spans.parse(members->value(QStringLiteral("k")))->toString(), QStringLiteral("a } ] [ { \" \\"));
        QCOMPARE(spans.parse(members->value(QStringLiteral("n")))->toArray().size(), 2);
        // Escaped key.
        const QByteArray escapedKey = "{\"a\\\"b\": 1}";
        QVERIFY(JsonSpans(escapedKey).objectMembers(*JsonSpans(escapedKey).root())->contains(QStringLiteral("a\"b")));
    }

    void lastDuplicateKeyWins()
    {
        const QByteArray text = "{\"a\": 1, \"a\": 2}";
        const JsonSpans spans(text);
        QCOMPARE(spans.parse(spans.objectMembers(*spans.root())->value(QStringLiteral("a")))->toInt(), 2);
    }

    void rejectsBrokenStructure_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("unbalanced") << QByteArray("{\"a\": [1, 2}");
        QTest::newRow("mismatched") << QByteArray("{\"a\": [1, 2}]");
        QTest::newRow("unterminated string") << QByteArray("{\"a\": \"x}");
        QTest::newRow("missing colon") << QByteArray("{\"a\" 1}");
        QTest::newRow("missing comma") << QByteArray("{\"a\": 1 \"b\": 2}");
        QTest::newRow("trailing comma") << QByteArray("{\"a\": 1,}");
        QTest::newRow("key not a string") << QByteArray("{a: 1}");
        QTest::newRow("trailing garbage") << QByteArray("{\"a\": 1} x");
    }

    void rejectsBrokenStructure()
    {
        QFETCH(QByteArray, text);
        const JsonSpans spans(text);
        const auto root = spans.root();
        QVERIFY(!root || !spans.objectMembers(*root));
    }

    // The scanner only delimits values; Qt's parser still validates each one.
    void parseValidatesTheValue()
    {
        const QByteArray text = "{\"a\": [1, 2,, 3], \"b\": tru}";
        const JsonSpans spans(text);
        const auto members = spans.objectMembers(*spans.root());
        QVERIFY(members);
        QVERIFY(!spans.parse(members->value(QStringLiteral("a"))));
        QVERIFY(!spans.parse(members->value(QStringLiteral("b"))));
    }
};

QTEST_GUILESS_MAIN(TestJsonSpans)
#include "tst_json_spans.moc"
