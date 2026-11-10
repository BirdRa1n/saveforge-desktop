#include "StaticHttpServer.h"

#include "savevault/registry/Fetcher.h"
#include "savevault/registry/RegistryUpdater.h"

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using namespace savevault;
using namespace savevault::registry;

namespace {

const QString kFixtures = QStringLiteral(SAVEVAULT_TEST_DATA_DIR "/registry");
const QString kBasic = kFixtures + QStringLiteral("/basic");
const QString kChannel = QStringLiteral("test-channel");
const QString kBundle = QStringLiteral("channels/test-channel/bundles/test-channel-000042.json.zst");

UpdateOptions options(const QStringList &channels = {kChannel})
{
    UpdateOptions o;
    o.channels = channels;
    o.appVersion = QStringLiteral("0.1.0");
    o.userAgent = QStringLiteral("SaveVault-test");
    return o;
}

RootMetadata anchor()
{
    return builtinTrustAnchor().value();
}

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

// A copy of the basic fixture whose index no longer matches its signature.
void makeTampered(const QString &dir)
{
    copyTree(kBasic, dir);
    QFile index(dir + QStringLiteral("/channels/test-channel/index.json"));
    QVERIFY(index.open(QIODevice::Append));
    index.write(" ");
}

QString describe(const ChannelUpdate &update)
{
    return update.error ? update.error->toString() : QStringLiteral("no error");
}

} // namespace

class TestRegistryUpdater : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Misused network APIs only warn at run time.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("^QNetwork")));
    }

    void acceptsOnlySafeMirrors()
    {
        QVERIFY(makeFetcher(QStringLiteral("https://registry.example.org/v1"), {}).ok());
        QVERIFY(makeFetcher(QStringLiteral("http://localhost:8080/v1/"), {}).ok());
        QVERIFY(makeFetcher(QStringLiteral("http://127.0.0.1:8080/"), {}).ok());
        QVERIFY(makeFetcher(kBasic, {}).ok());
        QVERIFY(makeFetcher(QUrl::fromLocalFile(kBasic).toString(), {}).ok());

        const auto plain = makeFetcher(QStringLiteral("http://registry.example.org/v1/"), {});
        QVERIFY(!plain.ok());
        QCOMPARE(plain.error().code, ErrorCode::InvalidArgument);
        QCOMPARE(makeFetcher(QStringLiteral("ftp://registry.example.org/"), {}).error().code, ErrorCode::InvalidArgument);
        QCOMPARE(makeFetcher(QString(), {}).error().code, ErrorCode::InvalidArgument);
        QCOMPARE(makeFetcher(kFixtures + QStringLiteral("/missing"), {}).error().code, ErrorCode::NotFound);
    }

    void fetchersEnforceTheSizeLimit()
    {
        test::StaticHttpServer server(kBasic);
        QVERIFY(server.listen());
        for (const QString &mirror : {kBasic, server.baseUrl()}) {
            auto fetcher = makeFetcher(mirror, {});
            QVERIFY(fetcher.ok());
            QVERIFY(fetcher.value()->fetch(QStringLiteral("root.json"), kMaxRegistryFileBytes).ok());
            QCOMPARE(fetcher.value()->fetch(QStringLiteral("root.json"), 10).error().code, ErrorCode::Corrupted);
            QCOMPARE(fetcher.value()->fetch(QStringLiteral("missing.json"), 10).error().code, ErrorCode::NotFound);
        }
    }

    void updatesFromADirectoryMirror()
    {
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        const auto results = updateFromMirrors({kBasic}, cache, anchor(), options());
        QCOMPARE(results.size(), 1);
        QVERIFY2(results.first().status == ChannelUpdate::Status::Updated, qPrintable(describe(results.first())));
        QCOMPARE(results.first().sequence, 42);
        QCOMPARE(results.first().profileCount, 1);
        QCOMPARE(results.first().mirror, kBasic);
        QCOMPARE(cache.lastSequence(kChannel), 42);
        // The staging directory is gone; only the verified copy stays.
        QCOMPARE(QDir(tmp.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden), (QStringList{kChannel}));
    }

    void downloadsOverHttpAndSkipsUnchangedBundles()
    {
        test::StaticHttpServer server(kBasic);
        QVERIFY(server.listen());
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());

        const auto first = updateFromMirrors({server.baseUrl()}, cache, anchor(), options());
        QVERIFY2(first.first().status == ChannelUpdate::Status::Updated, qPrintable(describe(first.first())));
        QVERIFY(server.requests().contains(kBundle));
        QVERIFY(server.requests().contains(QStringLiteral("roots/2.json"))); // looked for a newer root

        server.clearRequests();
        const auto second = updateFromMirrors({server.baseUrl()}, cache, anchor(), options());
        QCOMPARE(second.first().status, ChannelUpdate::Status::UpToDate);
        QCOMPARE(second.first().sequence, 42);
        QVERIFY(!server.requests().contains(kBundle));
    }

    void followsRootRotation()
    {
        test::StaticHttpServer server(kFixtures + QStringLiteral("/rotated"));
        QVERIFY(server.listen());
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        const auto results = updateFromMirrors({server.baseUrl()}, cache, anchor(), options());
        QVERIFY2(results.first().status == ChannelUpdate::Status::Updated, qPrintable(describe(results.first())));
        QVERIFY(QFile::exists(tmp.path() + QStringLiteral("/test-channel/42/roots/2.json")));
    }

    void fallsBackToTheNextMirror()
    {
        QTemporaryDir tampered;
        makeTampered(tampered.path());
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        const QStringList mirrors{QStringLiteral("http://example.org/plain-http-is-refused/"), tampered.path(), kBasic};
        const auto results = updateFromMirrors(mirrors, cache, anchor(), options());
        QVERIFY2(results.first().status == ChannelUpdate::Status::Updated, qPrintable(describe(results.first())));
        QCOMPARE(results.first().mirror, kBasic);
    }

    void keepsTheCacheWhenEveryMirrorFails()
    {
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        QCOMPARE(updateFromMirrors({kBasic}, cache, anchor(), options()).first().status, ChannelUpdate::Status::Updated);

        QTemporaryDir tampered;
        makeTampered(tampered.path());
        const auto results = updateFromMirrors({tampered.path()}, cache, anchor(), options());
        QCOMPARE(results.first().status, ChannelUpdate::Status::UpToDate); // same sequence: nothing to download

        // Without the accepted sequence the client wants the bundle, and verification refuses it.
        QFile::remove(tmp.path() + QStringLiteral("/state.json"));
        const auto retry = updateFromMirrors({tampered.path()}, cache, anchor(), options());
        QCOMPARE(retry.first().status, ChannelUpdate::Status::Failed);
        QCOMPARE(retry.first().error->code, ErrorCode::Untrusted);
        QVERIFY(cache.loadChannel(kChannel, anchor(), QStringLiteral("0.1.0")).ok());
    }

    void refusesRollbacks()
    {
        QTemporaryDir tmp;
        QFile state(tmp.path() + QStringLiteral("/state.json"));
        QVERIFY(state.open(QIODevice::WriteOnly));
        state.write("{\"channels\":{\"test-channel\":{\"lastSequence\":50}}}");
        state.close();

        test::StaticHttpServer server(kBasic);
        QVERIFY(server.listen());
        RegistryCache cache(tmp.path());
        const auto results = updateFromMirrors({server.baseUrl()}, cache, anchor(), options());
        QCOMPARE(results.first().status, ChannelUpdate::Status::Failed);
        QCOMPARE(results.first().error->code, ErrorCode::Untrusted);
        QVERIFY(!server.requests().contains(kBundle)); // decided before downloading the bundle
        QCOMPARE(cache.lastSequence(kChannel), 50);
    }

    void reportsMissingChannelsAndMirrors()
    {
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        const auto unpublished = updateFromMirrors({kBasic}, cache, anchor(), options({kChannel, QStringLiteral("official")}));
        QCOMPARE(unpublished.size(), 2);
        QCOMPARE(unpublished.at(0).status, ChannelUpdate::Status::Updated);
        QCOMPARE(unpublished.at(1).status, ChannelUpdate::Status::Failed);
        QCOMPARE(unpublished.at(1).error->code, ErrorCode::NotFound);

        const auto none = updateFromMirrors({}, cache, anchor(), options());
        QCOMPARE(none.first().status, ChannelUpdate::Status::Failed);
        QCOMPARE(none.first().error->code, ErrorCode::NotFound);
    }

    void importsADistributionOnlyWhenMissing()
    {
        QTemporaryDir tmp;
        RegistryCache cache(tmp.path());
        const auto first = importDistribution(kBasic, cache, anchor(), QStringLiteral("0.1.0"), {}, true);
        QCOMPARE(first.size(), 1);
        QCOMPARE(first.first().status, ChannelUpdate::Status::Updated);
        QVERIFY(importDistribution(kBasic, cache, anchor(), QStringLiteral("0.1.0"), {}, true).isEmpty());
        QCOMPARE(importDistribution(kBasic, cache, anchor(), QStringLiteral("0.1.0")).size(), 1);
    }

    void developmentBuildsUseThePublishedTestMirror()
    {
        QVERIFY(bundledDistribution().isEmpty());
        // Development builds trust the test keys and follow the matching published distribution.
        QCOMPARE(builtinMirrors(), QStringList{QStringLiteral("https://birdra1n.github.io/savevault-registry/v1/")});
        QVERIFY(makeFetcher(builtinMirrors().first(), {}).ok());
        QCOMPARE(defaultChannels(), (QStringList{QStringLiteral("official"), QStringLiteral("upstream-ludusavi")}));
    }
};

QTEST_GUILESS_MAIN(TestRegistryUpdater)
#include "tst_registry_updater.moc"
