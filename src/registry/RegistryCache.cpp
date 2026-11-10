#include "savevault/registry/RegistryCache.h"

#include "savevault/core/Uuid.h"
#include "savevault/platform/FileSync.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcRegistry, "savevault.registry")

namespace savevault::registry {

namespace {

// Files of a distribution that belong to one channel.
QStringList channelFiles(const QString &distributionDir, const QString &channel, const QString &bundlePath)
{
    QStringList files{QStringLiteral("root.json"), QStringLiteral("root.json.sig")};
    for (const QString &name : QDir(distributionDir + QStringLiteral("/roots")).entryList({QStringLiteral("*.json*")}, QDir::Files))
        files.append(QStringLiteral("roots/") + name);
    const QString base = QStringLiteral("channels/") + channel + u'/';
    files << base + QStringLiteral("index.json") << base + QStringLiteral("index.json.sig") << base + bundlePath;
    return files;
}

bool copyDurably(const QString &from, const QString &to)
{
    if (!QDir().mkpath(QFileInfo(to).absolutePath()))
        return false;
    QFile source(from);
    QFile target(to);
    if (!source.open(QIODevice::ReadOnly) || !target.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return false;
    const QByteArray bytes = source.readAll();
    return target.write(bytes) == bytes.size() && platform::syncFile(target);
}

} // namespace

Result<RootMetadata> builtinTrustAnchor()
{
    QFile file(QStringLiteral(":/savevault/registry/trust-anchor.json"));
    if (!file.open(QIODevice::ReadOnly))
        return Error{ErrorCode::NotFound, QStringLiteral("no registry trust anchor in this build"), {}};
    return parseRoot(file.readAll());
}

bool builtinTrustAnchorIsForTesting()
{
#if defined(SAVEVAULT_REGISTRY_TEST_ANCHOR)
    return true;
#else
    return false;
#endif
}

RegistryCache::RegistryCache(const QString &directory)
    : m_dir(QDir::cleanPath(directory))
{
}

qint64 RegistryCache::lastSequence(const QString &channel) const
{
    QFile file(m_dir + QStringLiteral("/state.json"));
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    return QJsonDocument::fromJson(file.readAll())
        .object()
        .value(QLatin1String("channels"))
        .toObject()
        .value(channel)
        .toObject()
        .value(QLatin1String("lastSequence"))
        .toInteger();
}

Result<void> RegistryCache::setLastSequence(const QString &channel, qint64 sequence)
{
    QJsonObject state;
    QFile current(m_dir + QStringLiteral("/state.json"));
    if (current.open(QIODevice::ReadOnly))
        state = QJsonDocument::fromJson(current.readAll()).object();
    current.close();

    QJsonObject channels = state.value(QLatin1String("channels")).toObject();
    channels.insert(channel, QJsonObject{{QStringLiteral("lastSequence"), sequence}});
    state.insert(QStringLiteral("channels"), channels);

    const QString tmp = m_dir + QStringLiteral("/state.json.") + uuidToString(createUuidV7()).left(8) + QStringLiteral(".tmp");
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return Error{ErrorCode::Io, QStringLiteral("cannot write registry state"), tmp};
    const QByteArray bytes = QJsonDocument(state).toJson();
    if (out.write(bytes) != bytes.size() || !platform::syncFile(out)) {
        out.close();
        QFile::remove(tmp);
        return Error{ErrorCode::Io, QStringLiteral("cannot write registry state"), tmp};
    }
    out.close();
    QFile::remove(m_dir + QStringLiteral("/state.json"));
    if (!QFile::rename(tmp, m_dir + QStringLiteral("/state.json")))
        return Error{ErrorCode::Io, QStringLiteral("cannot replace registry state"), m_dir};
    platform::syncDirectory(m_dir);
    return {};
}

QList<qint64> RegistryCache::cachedSequences(const QString &channel) const
{
    QList<qint64> sequences;
    for (const QString &name : QDir(m_dir + u'/' + channel).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        const qint64 sequence = name.toLongLong(&numeric);
        if (numeric)
            sequences.append(sequence);
    }
    std::sort(sequences.begin(), sequences.end(), std::greater<>());
    return sequences;
}

QStringList RegistryCache::channels() const
{
    QStringList names;
    for (const QString &name : QDir(m_dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (!cachedSequences(name).isEmpty())
            names.append(name);
    }
    return names;
}

Result<VerifiedChannel> RegistryCache::importChannel(const QString &distributionDir, const QString &channel,
                                                     const RootMetadata &anchor, const QString &appVersion)
{
    LoadOptions options;
    options.minimumSequence = lastSequence(channel);
    options.appVersion = appVersion;
    auto verified = registry::loadChannel(distributionDir, channel, anchor, options);
    if (!verified.ok())
        return verified;

    const qint64 sequence = verified.value().index.sequence;
    const QString target = m_dir + u'/' + channel + u'/' + QString::number(sequence);
    if (!QDir(target).exists()) {
        // Copy into a temporary sibling, then rename: a crash never leaves a half copy.
        const QString staging = target + QStringLiteral(".") + uuidToString(createUuidV7()).left(8) + QStringLiteral(".tmp");
        for (const QString &file : channelFiles(distributionDir, channel, verified.value().index.bundlePath)) {
            if (!QFile::exists(distributionDir + u'/' + file))
                continue;
            if (!copyDurably(distributionDir + u'/' + file, staging + u'/' + file)) {
                QDir(staging).removeRecursively();
                return Error{ErrorCode::Io, QStringLiteral("cannot copy registry files into the cache"), file};
            }
        }
        if (!QDir().rename(staging, target)) {
            QDir(staging).removeRecursively();
            return Error{ErrorCode::Io, QStringLiteral("cannot store the channel in the cache"), target};
        }
        platform::syncDirectory(m_dir + u'/' + channel);
    }
    if (sequence > options.minimumSequence) {
        if (auto saved = setLastSequence(channel, sequence); !saved.ok())
            return saved.error();
    }

    // Keep the newest versions only.
    const QList<qint64> sequences = cachedSequences(channel);
    for (qsizetype i = kKeptVersions; i < sequences.size(); ++i)
        QDir(m_dir + u'/' + channel + u'/' + QString::number(sequences.at(i))).removeRecursively();

    qCInfo(lcRegistry) << "cached channel" << channel << "sequence" << sequence << "with"
                       << verified.value().profiles.size() << "profiles";
    return verified;
}

Result<VerifiedChannel> RegistryCache::loadChannel(const QString &channel, const RootMetadata &anchor,
                                                   const QString &appVersion) const
{
    LoadOptions options;
    options.appVersion = appVersion;
    std::optional<Error> firstError;
    for (qint64 sequence : cachedSequences(channel)) {
        auto verified = registry::loadChannel(m_dir + u'/' + channel + u'/' + QString::number(sequence), channel, anchor,
                                              options);
        if (verified.ok())
            return verified;
        qCWarning(lcRegistry) << "cached" << channel << sequence << "is unusable:" << verified.error().toString();
        if (!firstError)
            firstError = verified.error();
    }
    return firstError ? *firstError
                      : Error{ErrorCode::NotFound, QStringLiteral("channel is not in the cache"), channel};
}

} // namespace savevault::registry
