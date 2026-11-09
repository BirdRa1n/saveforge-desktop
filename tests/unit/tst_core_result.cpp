#include "saveforge/core/Result.h"
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
