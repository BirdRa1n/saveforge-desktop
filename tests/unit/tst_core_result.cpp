#include "saveforge/core/Result.h"
#include "saveforge/core/Uuid.h"
#include "saveforge/core/Version.h"

#include <QTest>

using saveforge::Error;
using saveforge::ErrorCode;
using saveforge::Result;

namespace {

Result<int> parsePositive(int value)
{
    if (value <= 0)
        return Error{ErrorCode::InvalidArgument, QStringLiteral("valor deve ser positivo"), QString::number(value)};
    return value;
}

Result<void> succeedIf(bool condition)
{
    if (!condition)
        return Error{ErrorCode::Cancelled, QStringLiteral("cancelado"), {}};
    return {};
}

} // namespace

class TestCoreResult : public QObject {
    Q_OBJECT

private slots:
    void holdsValue()
    {
        const auto r = parsePositive(42);
        QVERIFY(r.ok());
        QVERIFY(static_cast<bool>(r));
        QCOMPARE(r.value(), 42);
    }

    void holdsError()
    {
        const auto r = parsePositive(-1);
        QVERIFY(!r.ok());
        QCOMPARE(r.error().code, ErrorCode::InvalidArgument);
        QCOMPARE(r.error().context, QStringLiteral("-1"));
        QCOMPARE(r.valueOr(7), 7);
    }

    void movesValueOut()
    {
        Result<QString> r = QStringLiteral("save.dat");
        const QString moved = std::move(r).value();
        QCOMPARE(moved, QStringLiteral("save.dat"));
    }

    void voidResult()
    {
        QVERIFY(succeedIf(true).ok());
        const auto r = succeedIf(false);
        QVERIFY(!r.ok());
        QCOMPARE(r.error().code, ErrorCode::Cancelled);
    }

    void errorToString()
    {
        const Error e{ErrorCode::NotFound, QStringLiteral("arquivo ausente"), QStringLiteral("C:/x.sav")};
        QCOMPARE(e.toString(), QStringLiteral("not_found: arquivo ausente (C:/x.sav)"));
        const Error noContext{ErrorCode::Io, QStringLiteral("falha de leitura"), {}};
        QCOMPARE(noContext.toString(), QStringLiteral("io: falha de leitura"));
    }

    void uuidV7Layout()
    {
        const QDateTime when = QDateTime::fromString(QStringLiteral("2026-11-09T12:00:00.123Z"), Qt::ISODateWithMs);
        const QUuid id = saveforge::createUuidV7(when);
        QCOMPARE(id.version(), QUuid::Version(7));
        QCOMPARE(id.variant(), QUuid::DCE);

        // The first 48 bits are the Unix timestamp in milliseconds.
        const QByteArray raw = id.toRfc4122();
        quint64 millis = 0;
        for (int i = 0; i < 6; ++i)
            millis = (millis << 8) | static_cast<quint8>(raw.at(i));
        QCOMPARE(static_cast<qint64>(millis), when.toMSecsSinceEpoch());

        const QString text = saveforge::uuidToString(id);
        QCOMPARE(text.size(), 36);
        QCOMPARE(text, text.toLower());
    }

    void uuidV7SortsByTime()
    {
        const QDateTime t0 = QDateTime::currentDateTimeUtc();
        const QString earlier = saveforge::uuidToString(saveforge::createUuidV7(t0));
        const QString later = saveforge::uuidToString(saveforge::createUuidV7(t0.addMSecs(1)));
        QVERIFY(earlier < later);
        QVERIFY(saveforge::createUuidV7(t0) != saveforge::createUuidV7(t0)); // random part differs
    }

    void versionIsSemver()
    {
        QCOMPARE(saveforge::kAppName, std::string_view("SaveForge"));
        const QString version = QString::fromUtf8(saveforge::kVersion.data(),
                                                  static_cast<qsizetype>(saveforge::kVersion.size()));
        QCOMPARE(version.split(QLatin1Char('.')).size(), 3);
    }
};

QTEST_GUILESS_MAIN(TestCoreResult)
#include "tst_core_result.moc"
