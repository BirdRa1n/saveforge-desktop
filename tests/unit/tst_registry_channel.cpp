// Channel verification against signed fixtures (tests/data/registry, generated
// by generate.mjs with the public test keys of saveforge-specs).

#include "saveforge/registry/Channel.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace saveforge;
using namespace saveforge::registry;

namespace {

const QString kFixtures = QStringLiteral(SAVEFORGE_TEST_DATA_DIR "/registry");

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void write(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

// The trust anchor a build would embed: root version 1 of the fixtures.
RootMetadata anchor()
{
    auto root = parseRoot(read(kFixtures + QStringLiteral("/basic/roots/1.json")));
    if (!root.ok())
        qFatal("fixture root is invalid: %s", qPrintable(root.error().toString()));
    return root.value();
}

// A writable copy of a fixture distribution.
QString copyFixture(const QTemporaryDir &tmp, const QString &name)
{
    const QString source = kFixtures + u'/' + name;
    QDirIterator it(source, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString target = tmp.path() + file.mid(source.size());
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::copy(file, target);
    }
    return tmp.path();
}

LoadOptions options(qint64 minimumSequence = 0, const QString &appVersion = QStringLiteral("0.1.0"))
{
    LoadOptions o;
    o.minimumSequence = minimumSequence;
    o.appVersion = appVersion;
    o.now = QDateTime::fromString(QStringLiteral("2026-12-01T00:00:00Z"), Qt::ISODate);
    return o;
}

} // namespace

class TestRegistryChannel : public QObject {
    Q_OBJECT

private slots:
    void loadsAndFiltersProfiles()
    {
        const auto channel = loadChannel(kFixtures + QStringLiteral("/basic"), QStringLiteral("test-channel"), anchor(), options());
        QVERIFY2(channel.ok(), qPrintable(channel.ok() ? QString() : channel.error().toString()));
        const VerifiedChannel &c = channel.value();
        QCOMPARE(c.index.sequence, 42);
        QVERIFY(!c.expired);
        QCOMPARE(c.profiles.size(), 1);
        QCOMPARE(c.profiles.first().id, QStringLiteral("hollow-knight"));
        QCOMPARE(c.rejectedProfiles.size(), 2); // unsafe path + revoked
        QVERIFY(c.rejectedProfiles.join(u'\n').contains(QStringLiteral("unsafe-game")));
        QVERIFY(c.rejectedProfiles.join(u'\n').contains(QStringLiteral("revoked-game: revoked")));
        QCOMPARE(c.notices.first().attribution, QStringLiteral("SaveForge test fixture"));
    }

    void followsRootRotation()
    {
        const auto channel = loadChannel(kFixtures + QStringLiteral("/rotated"), QStringLiteral("test-channel"), anchor(), options());
        QVERIFY2(channel.ok(), qPrintable(channel.ok() ? QString() : channel.error().toString()));
        QCOMPARE(channel.value().root.version, 2);
    }

    void rejectsATamperedIndex()
    {
        QTemporaryDir tmp;
        const QString dir = copyFixture(tmp, QStringLiteral("basic"));
        const QString index = dir + QStringLiteral("/channels/test-channel/index.json");
        write(index, read(index).replace("\"sequence\": 42", "\"sequence\": 43"));
        const auto channel = loadChannel(dir, QStringLiteral("test-channel"), anchor(), options());
        QVERIFY(!channel.ok());
        QCOMPARE(channel.error().code, ErrorCode::Untrusted);
    }

    void rejectsASwappedBundle()
    {
        QTemporaryDir tmp;
        const QString dir = copyFixture(tmp, QStringLiteral("basic"));
        const QString bundle = dir + QStringLiteral("/channels/test-channel/bundles/test-channel-000042.json.zst");
        QByteArray bytes = read(bundle);
        bytes[bytes.size() / 2] = static_cast<char>(bytes.at(bytes.size() / 2) ^ 0x01);
        write(bundle, bytes);
        const auto channel = loadChannel(dir, QStringLiteral("test-channel"), anchor(), options());
        QVERIFY(!channel.ok());
        QCOMPARE(channel.error().code, ErrorCode::Untrusted);
    }

    void rejectsRollback()
    {
        const auto channel = loadChannel(kFixtures + QStringLiteral("/basic"), QStringLiteral("test-channel"), anchor(), options(43));
        QVERIFY(!channel.ok());
        QCOMPARE(channel.error().code, ErrorCode::Untrusted);
        QVERIFY(loadChannel(kFixtures + QStringLiteral("/basic"), QStringLiteral("test-channel"), anchor(), options(42)).ok());
    }

    void rejectsARootNotSignedByThePreviousKeys()
    {
        QTemporaryDir tmp;
        const QString dir = copyFixture(tmp, QStringLiteral("rotated"));
        // Replace the rotation's signature with one from the new (untrusted) key's index signature.
        write(dir + QStringLiteral("/roots/2.json.sig"), read(dir + QStringLiteral("/channels/test-channel/index.json.sig")));
        const auto channel = loadChannel(dir, QStringLiteral("test-channel"), anchor(), options());
        QVERIFY(!channel.ok());
        QCOMPARE(channel.error().code, ErrorCode::Untrusted);
    }

    void rejectsIndexesSignedByAKeyForAnotherChannel()
    {
        // In "rotated", the online key is limited to "test-channel".
        QTemporaryDir tmp;
        const QString dir = copyFixture(tmp, QStringLiteral("rotated"));
        QDir(dir + QStringLiteral("/channels")).rename(QStringLiteral("test-channel"), QStringLiteral("other-channel"));
        const auto channel = loadChannel(dir, QStringLiteral("other-channel"), anchor(), options());
        QVERIFY(!channel.ok());
        QCOMPARE(channel.error().code, ErrorCode::Untrusted);
    }

    void requiresASufficientAppVersion()
    {
        const auto channel = loadChannel(kFixtures + QStringLiteral("/basic"), QStringLiteral("test-channel"), anchor(),
                                         options(0, QStringLiteral("0.0.9")));
        QVERIFY(!channel.ok());
        QCOMPARE(channel.error().code, ErrorCode::Unsupported);
    }

    void flagsExpiredIndexesWithoutFailing()
    {
        LoadOptions late = options();
        late.now = QDateTime::fromString(QStringLiteral("2028-01-01T00:00:00Z"), Qt::ISODate);
        const auto channel = loadChannel(kFixtures + QStringLiteral("/basic"), QStringLiteral("test-channel"), anchor(), late);
        QVERIFY(channel.ok());
        QVERIFY(channel.value().expired);
    }

    void reportsMissingChannels()
    {
        const auto channel = loadChannel(kFixtures + QStringLiteral("/basic"), QStringLiteral("official"), anchor(), options());
        QVERIFY(!channel.ok());
        QCOMPARE(channel.error().code, ErrorCode::NotFound);
    }

    void comparesVersions()
    {
        QCOMPARE(compareVersions(QStringLiteral("0.1.0"), QStringLiteral("0.1.0")), 0);
        QCOMPARE(compareVersions(QStringLiteral("0.2.0"), QStringLiteral("0.10.0")), -1);
        QCOMPARE(compareVersions(QStringLiteral("1.0.0-beta"), QStringLiteral("0.9.9")), 1);
    }
};

QTEST_GUILESS_MAIN(TestRegistryChannel)
#include "tst_registry_channel.moc"
