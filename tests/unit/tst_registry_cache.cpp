#include "savevault/registry/RegistryCache.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace savevault;
using namespace savevault::registry;

namespace {

const QString kBasic = QStringLiteral(SAVEVAULT_TEST_DATA_DIR "/registry/basic");
const QString kChannel = QStringLiteral("test-channel");
const QString kVersion = QStringLiteral("0.1.0");

void copyTree(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString target = to + file.mid(from.size());
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::copy(file, target);
    }
}

} // namespace

class TestRegistryCache : public QObject {
    Q_OBJECT

private slots:
    void embedsATrustAnchor()
    {
        const auto anchor = builtinTrustAnchor();
        QVERIFY2(anchor.ok(), qPrintable(anchor.ok() ? QString() : anchor.error().toString()));
        QCOMPARE(anchor.value().version, 1);
        QVERIFY(builtinTrustAnchorIsForTesting()); // default development build
    }

    void importsAndLoadsAChannel()
    {
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        const RootMetadata anchor = builtinTrustAnchor().value();

        const auto imported = cache.importChannel(kBasic, kChannel, anchor, kVersion);
        QVERIFY2(imported.ok(), qPrintable(imported.ok() ? QString() : imported.error().toString()));
        QCOMPARE(cache.lastSequence(kChannel), 42);
        QCOMPARE(cache.channels(), QStringList{kChannel});
        QVERIFY(QFile::exists(tmp.path() + QStringLiteral("/test-channel/42/channels/test-channel/index.json")));

        // Importing the same sequence again is harmless.
        QVERIFY(cache.importChannel(kBasic, kChannel, anchor, kVersion).ok());

        const auto loaded = cache.loadChannel(kChannel, anchor, kVersion);
        QVERIFY(loaded.ok());
        QCOMPARE(loaded.value().profiles.size(), 1);
        QVERIFY(!cache.loadChannel(QStringLiteral("official"), anchor, kVersion).ok());
    }

    void refusesOlderSequencesThanAccepted()
    {
        QTemporaryDir tmp;
        QFile state(tmp.path() + QStringLiteral("/state.json"));
        QVERIFY(state.open(QIODevice::WriteOnly));
        state.write(R"({"channels":{"test-channel":{"lastSequence":50}}})");
        state.close();

        RegistryCache cache(tmp.path());
        const auto imported = cache.importChannel(kBasic, kChannel, builtinTrustAnchor().value(), kVersion);
        QVERIFY(!imported.ok());
        QCOMPARE(imported.error().code, ErrorCode::Untrusted);
        QCOMPARE(cache.lastSequence(kChannel), 50);
    }

    void refusesUntrustedDistributions()
    {
        QTemporaryDir source;
        copyTree(kBasic, source.path());
        QFile index(source.path() + QStringLiteral("/channels/test-channel/index.json"));
        QVERIFY(index.open(QIODevice::Append));
        index.write(" ");
        index.close();

        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        QVERIFY(!cache.importChannel(source.path(), kChannel, builtinTrustAnchor().value(), kVersion).ok());
        QVERIFY(cache.channels().isEmpty());
    }

    void keepsTheNewestVersionsAndFallsBack()
    {
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        const RootMetadata anchor = builtinTrustAnchor().value();
        // Older cached versions (same verified content under older sequence numbers).
        for (const char *older : {"1", "2", "3"})
            copyTree(kBasic, tmp.path() + QStringLiteral("/test-channel/") + QLatin1String(older));
        QVERIFY(cache.importChannel(kBasic, kChannel, anchor, kVersion).ok());
        QCOMPARE(QDir(tmp.path() + QStringLiteral("/test-channel")).entryList(QDir::Dirs | QDir::NoDotAndDotDot),
                 (QStringList{QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("42")}));

        // Damage the newest copy: loading falls back to the next one.
        QFile bundle(tmp.path() + QStringLiteral("/test-channel/42/channels/test-channel/bundles/test-channel-000042.json.zst"));
        QVERIFY(bundle.open(QIODevice::WriteOnly | QIODevice::Truncate));
        bundle.write("damaged");
        bundle.close();
        const auto loaded = cache.loadChannel(kChannel, anchor, kVersion);
        QVERIFY(loaded.ok());
        QCOMPARE(loaded.value().profiles.first().id, QStringLiteral("hollow-knight"));
    }
};

QTEST_GUILESS_MAIN(TestRegistryCache)
#include "tst_registry_cache.moc"
