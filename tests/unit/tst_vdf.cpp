#include "saveforge/detection/Vdf.h"

#include <QTest>

using namespace saveforge;
using namespace saveforge::detection;

class TestVdf : public QObject {
    Q_OBJECT

private slots:
    void parsesAppManifest()
    {
        const auto result = parseVdf("\n"
            "\"AppState\"\n"
            "{\n"
            "\t\"appid\"\t\t\"367520\"\n"
            "\t\"Universe\"\t\t\"1\"\n"
            "\t\"name\"\t\t\"Hollow Knight\"\n"
            "\t\"StateFlags\"\t\t\"4\"\n"
            "\t\"installdir\"\t\t\"Hollow Knight\"\n"
            "\t\"UserConfig\"\n"
            "\t{\n"
            "\t\t\"language\"\t\t\"brazilian\"\n"
            "\t}\n"
            "}\n"
            "");
        QVERIFY(result.ok());
        const VdfNode *app = result.value().child(u"appstate"); // keys are case-insensitive
        QVERIFY(app && app->isObject);
        QCOMPARE(app->valueOf(u"appid"), QStringLiteral("367520"));
        QCOMPARE(app->valueOf(u"INSTALLDIR"), QStringLiteral("Hollow Knight"));
        QCOMPARE(app->child(u"UserConfig")->valueOf(u"language"), QStringLiteral("brazilian"));
        QCOMPARE(app->valueOf(u"missing", QStringLiteral("x")), QStringLiteral("x"));
    }

    void handlesEscapesCommentsAndConditionals()
    {
        const auto result = parseVdf("\n"
            "// libraryfolders.vdf written on Windows\n"
            "\"libraryfolders\"\n"
            "{\n"
            "\t\"0\"\n"
            "\t{\n"
            "\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n"
            "\t\t\"label\"\t\t\"say \\\"hi\\\"\"\n"
            "\t}\n"
            "\t\"flag\"\t\"1\"\t[$WIN32]\n"
            "\tunquoted\tvalue\n"
            "}\n"
            "");
        QVERIFY2(result.ok(), qPrintable(result.ok() ? QString() : result.error().toString()));
        const VdfNode *root = result.value().child(u"libraryfolders");
        QCOMPARE(root->child(u"0")->valueOf(u"path"), QStringLiteral("C:\\Program Files (x86)\\Steam"));
        QCOMPARE(root->child(u"0")->valueOf(u"label"), QStringLiteral("say \"hi\""));
        QCOMPARE(root->valueOf(u"flag"), QStringLiteral("1"));
        QCOMPARE(root->valueOf(u"unquoted"), QStringLiteral("value"));
    }

    void acceptsUtf8AndBom()
    {
        const auto result = parseVdf("\xEF\xBB\xBF\"AppState\" { \"name\" \"Ōkami HD\" }");
        QVERIFY(result.ok());
        QCOMPARE(result.value().child(u"AppState")->valueOf(u"name"), QString::fromUtf8("Ōkami HD"));
    }

    void rejectsMalformedInput_data()
    {
        QTest::addColumn<QByteArray>("input");
        QTest::newRow("unterminated string") << QByteArray("\"AppState\" { \"name\" \"Hollow");
        QTest::newRow("missing brace") << QByteArray("\"AppState\" { \"name\" \"x\"");
        QTest::newRow("extra brace") << QByteArray("\"a\" \"b\" }");
        QTest::newRow("key without value") << QByteArray("\"AppState\" { \"name\" }");
        QTest::newRow("too deep") << QByteArray(200, '{').prepend("\"k\" ");
    }
    void rejectsMalformedInput()
    {
        QFETCH(QByteArray, input);
        const auto result = parseVdf(input);
        QVERIFY(!result.ok());
        QCOMPARE(result.error().code, ErrorCode::Corrupted);
    }

    void emptyInputIsAnEmptyObject()
    {
        const auto result = parseVdf("  // nothing here\n");
        QVERIFY(result.ok());
        QVERIFY(result.value().children.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestVdf)
#include "tst_vdf.moc"
