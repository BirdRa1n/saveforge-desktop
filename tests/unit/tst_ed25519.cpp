// Conformance test: Ed25519 verification against
// saveforge-specs/registry/test-vectors/signing.json.

#include "saveforge/registry/Ed25519.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace saveforge::registry;

namespace {

QJsonObject vectors()
{
    QFile file(QStringLiteral(SAVEFORGE_SPECS_DIR "/registry/test-vectors/signing.json"));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("cannot open signing.json; is the specs submodule initialized?");
    return QJsonDocument::fromJson(file.readAll()).object();
}

QByteArray publicKeyFor(const QString &keyId)
{
    for (const QJsonValue &key : vectors().value(QLatin1String("keys")).toArray()) {
        if (key.toObject().value(QLatin1String("keyId")).toString() == keyId)
            return QByteArray::fromBase64(key.toObject().value(QLatin1String("publicKey")).toString().toLatin1());
    }
    return {};
}

} // namespace

class TestEd25519 : public QObject {
    Q_OBJECT

private slots:
    void keyIdsMatchTheSpec()
    {
        for (const QJsonValue &value : vectors().value(QLatin1String("keys")).toArray()) {
            const QJsonObject key = value.toObject();
            const QByteArray raw = QByteArray::fromBase64(key.value(QLatin1String("publicKey")).toString().toLatin1());
            QCOMPARE(keyIdOf(raw), key.value(QLatin1String("keyId")).toString());
        }
    }

    void verifiesVectors_data()
    {
        QTest::addColumn<QByteArray>("publicKey");
        QTest::addColumn<QByteArray>("message");
        QTest::addColumn<QByteArray>("signature");
        QTest::addColumn<bool>("valid");
        const QJsonArray cases = vectors().value(QLatin1String("cases")).toArray();
        QVERIFY(cases.size() >= 4);
        for (const QJsonValue &value : cases) {
            const QJsonObject c = value.toObject();
            QTest::newRow(qPrintable(c.value(QLatin1String("description")).toString()))
                << publicKeyFor(c.value(QLatin1String("keyId")).toString())
                << QByteArray::fromBase64(c.value(QLatin1String("message")).toString().toLatin1())
                << QByteArray::fromBase64(c.value(QLatin1String("signature")).toString().toLatin1())
                << c.value(QLatin1String("valid")).toBool();
        }
    }
    void verifiesVectors()
    {
        QFETCH(QByteArray, publicKey);
        QFETCH(QByteArray, message);
        QFETCH(QByteArray, signature);
        QFETCH(bool, valid);
        QCOMPARE(verifyEd25519(publicKey, message, signature), valid);
    }

    void rejectsMalformedInputs()
    {
        const QByteArray key(32, '\x01');
        QVERIFY(!verifyEd25519(QByteArray(31, '\x01'), "m", QByteArray(64, '\0')));
        QVERIFY(!verifyEd25519(key, "m", QByteArray(63, '\0')));
        QVERIFY(!verifyEd25519(key, "m", QByteArray(64, '\0')));
    }
};

QTEST_GUILESS_MAIN(TestEd25519)
#include "tst_ed25519.moc"
