#include "saveforge/registry/RegistryUpdater.h"

#include "saveforge/core/Uuid.h"
#include "saveforge/registry/Fetcher.h"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>

#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(lcRegistry)

namespace saveforge::registry {

namespace {

// Signature files and indexes are small; a cap keeps a hostile mirror from
// filling memory before verification.
constexpr qint64 kMaxMetadataBytes = 1024 * 1024;
// Root versions followed per update; real registries rotate keys rarely.
constexpr int kMaxRootVersions = 256;

// A scratch directory inside the cache that receives one mirror's files laid
// out as a distribution, so the regular verification can read them.
class Staging {
public:
    explicit Staging(const QString &cacheDir)
        : m_dir(cacheDir + QStringLiteral("/.staging-") + uuidToString(createUuidV7()).right(12))
    {
    }
    ~Staging() { QDir(m_dir).removeRecursively(); }
    Staging(const Staging &) = delete;
    Staging &operator=(const Staging &) = delete;

    [[nodiscard]] const QString &dir() const { return m_dir; }

    [[nodiscard]] Result<void> write(const QString &relativePath, const QByteArray &bytes) const
    {
        const QString path = m_dir + u'/' + relativePath;
        if (!QDir().mkpath(QFileInfo(path).absolutePath()))
            return Error{ErrorCode::Io, QStringLiteral("cannot create a staging directory"), path};
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes) != bytes.size())
            return Error{ErrorCode::Io, QStringLiteral("cannot write a downloaded file"), path};
        return {};
    }

private:
    QString m_dir;
};

// Downloads `path` into the staging directory. A missing file is reported as
// `false`, so callers can treat optional files as absent.
Result<bool> stage(Fetcher &fetcher, const Staging &staging, const QString &path, qint64 maxBytes)
{
    auto bytes = fetcher.fetch(path, maxBytes);
    if (!bytes.ok())
        return bytes.error().code == ErrorCode::NotFound ? Result<bool>(false) : Result<bool>(bytes.error());
    if (auto written = staging.write(path, bytes.value()); !written.ok())
        return written.error();
    return true;
}

Result<bool> stageSigned(Fetcher &fetcher, const Staging &staging, const QString &path)
{
    auto file = stage(fetcher, staging, path, kMaxMetadataBytes);
    if (!file.ok() || !file.value())
        return file;
    auto sig = stage(fetcher, staging, path + QStringLiteral(".sig"), kMaxMetadataBytes);
    if (!sig.ok())
        return sig.error();
    if (!sig.value())
        return Error{ErrorCode::Untrusted, QStringLiteral("signature file is missing"), path + QStringLiteral(".sig")};
    return true;
}

// root.json and every root version from the anchor's on (spec sections 1 and 3).
Result<void> stageRoots(Fetcher &fetcher, const Staging &staging, const RootMetadata &anchor)
{
    if (auto root = stageSigned(fetcher, staging, QStringLiteral("root.json")); !root.ok())
        return root.error();
    for (int version = anchor.version; version < anchor.version + kMaxRootVersions; ++version) {
        auto staged = stageSigned(fetcher, staging, QStringLiteral("roots/%1.json").arg(version));
        if (!staged.ok())
            return staged.error();
        // The anchor's own version is optional; the first missing newer one ends the chain.
        if (!staged.value() && version > anchor.version)
            return {};
    }
    return {};
}

ChannelUpdate failed(const QString &channel, const Error &error, const QString &mirror)
{
    ChannelUpdate update;
    update.channel = channel;
    update.error = error;
    update.mirror = mirror;
    return update;
}

ChannelUpdate updateChannel(Fetcher &fetcher, const Staging &staging, const QString &channel, RegistryCache &cache,
                            const RootMetadata &anchor, const QString &appVersion)
{
    const QString base = QStringLiteral("channels/") + channel + u'/';
    auto staged = stageSigned(fetcher, staging, base + QStringLiteral("index.json"));
    if (!staged.ok())
        return failed(channel, staged.error(), fetcher.location());
    if (!staged.value())
        return failed(channel, Error{ErrorCode::NotFound, QStringLiteral("channel is not published"), channel},
                      fetcher.location());

    // Peek at the index before verification only to decide what to download;
    // importChannel() verifies everything before the cache changes.
    QFile indexFile(staging.dir() + u'/' + base + QStringLiteral("index.json"));
    if (!indexFile.open(QIODevice::ReadOnly))
        return failed(channel, Error{ErrorCode::Io, indexFile.errorString(), indexFile.fileName()}, fetcher.location());
    auto index = parseIndex(indexFile.readAll());
    indexFile.close();
    if (!index.ok())
        return failed(channel, index.error(), fetcher.location());
    if (index.value().channel != channel)
        return failed(channel, Error{ErrorCode::Untrusted, QStringLiteral("index belongs to another channel"), channel},
                      fetcher.location());

    const qint64 last = cache.lastSequence(channel);
    if (index.value().sequence < last)
        return failed(channel,
                      Error{ErrorCode::Untrusted,
                            QStringLiteral("mirror offers sequence %1, older than the accepted %2")
                                .arg(index.value().sequence)
                                .arg(last),
                            channel},
                      fetcher.location());
    if (index.value().sequence == last && cache.loadChannel(channel, anchor, appVersion).ok()) {
        ChannelUpdate update;
        update.channel = channel;
        update.status = ChannelUpdate::Status::UpToDate;
        update.sequence = last;
        update.mirror = fetcher.location();
        return update;
    }

    if (index.value().bundleSize > kMaxRegistryFileBytes)
        return failed(channel, Error{ErrorCode::Corrupted, QStringLiteral("bundle is too large"), channel},
                      fetcher.location());
    // parseIndex() only accepts "bundles/<name>.json.zst", so the path stays inside the staging directory.
    auto bundle = stage(fetcher, staging, base + index.value().bundlePath, index.value().bundleSize);
    if (!bundle.ok())
        return failed(channel, bundle.error(), fetcher.location());
    if (!bundle.value())
        return failed(channel, Error{ErrorCode::NotFound, QStringLiteral("bundle is missing"), index.value().bundlePath},
                      fetcher.location());

    auto imported = cache.importChannel(staging.dir(), channel, anchor, appVersion);
    if (!imported.ok())
        return failed(channel, imported.error(), fetcher.location());
    ChannelUpdate update;
    update.channel = channel;
    update.status = ChannelUpdate::Status::Updated;
    update.sequence = imported.value().index.sequence;
    update.profileCount = imported.value().profiles.size();
    update.expired = imported.value().expired;
    update.mirror = fetcher.location();
    return update;
}

} // namespace

QStringList defaultChannels()
{
    return {QStringLiteral("official"), QStringLiteral("upstream-ludusavi")};
}

QStringList builtinMirrors()
{
#if defined(SAVEFORGE_REGISTRY_MIRRORS)
    QStringList mirrors = QStringLiteral(SAVEFORGE_REGISTRY_MIRRORS).split(u';', Qt::SkipEmptyParts);
    for (QString &mirror : mirrors)
        mirror = mirror.trimmed();
    mirrors.removeAll(QString());
    return mirrors;
#else
    return {};
#endif
}

QString bundledDistribution()
{
    const QString dir = QStringLiteral(":/saveforge/registry/bundled");
    return QFile::exists(dir + QStringLiteral("/root.json")) ? dir : QString();
}

QList<ChannelUpdate> updateFromMirrors(const QStringList &mirrors, RegistryCache &cache, const RootMetadata &anchor,
                                       const UpdateOptions &options)
{
    QList<ChannelUpdate> results;
    for (const QString &channel : options.channels)
        results.append(failed(channel, Error{ErrorCode::NotFound, QStringLiteral("no registry mirror is configured"), {}}, {}));
    if (!QDir().mkpath(cache.directory())) {
        for (ChannelUpdate &result : results)
            result.error = Error{ErrorCode::Io, QStringLiteral("cannot create the registry cache"), cache.directory()};
        return results;
    }

    for (const QString &mirror : mirrors) {
        const bool pending = std::any_of(results.begin(), results.end(), [](const ChannelUpdate &r) {
            return r.status == ChannelUpdate::Status::Failed;
        });
        if (!pending)
            break;

        auto fetcher = makeFetcher(mirror, options.userAgent);
        if (!fetcher.ok()) {
            for (ChannelUpdate &result : results) {
                if (result.status == ChannelUpdate::Status::Failed)
                    result = failed(result.channel, fetcher.error(), mirror);
            }
            continue;
        }
        Staging staging(cache.directory());
        if (auto roots = stageRoots(*fetcher.value(), staging, anchor); !roots.ok()) {
            qCWarning(lcRegistry) << "mirror" << mirror << "failed:" << roots.error().toString();
            for (ChannelUpdate &result : results) {
                if (result.status == ChannelUpdate::Status::Failed)
                    result = failed(result.channel, roots.error(), mirror);
            }
            continue;
        }
        for (ChannelUpdate &result : results) {
            if (result.status != ChannelUpdate::Status::Failed)
                continue;
            ChannelUpdate attempt = updateChannel(*fetcher.value(), staging, result.channel, cache, anchor, options.appVersion);
            if (attempt.status == ChannelUpdate::Status::Failed)
                qCWarning(lcRegistry) << "channel" << result.channel << "from" << mirror << "failed:"
                                      << attempt.error->toString();
            // Keep the most telling error: "not published" from one mirror must
            // not hide a real failure reported by another.
            if (attempt.status != ChannelUpdate::Status::Failed || !result.error
                || result.error->code == ErrorCode::NotFound || attempt.error->code != ErrorCode::NotFound)
                result = std::move(attempt);
        }
    }
    return results;
}

QList<ChannelUpdate> importDistribution(const QString &distributionDir, RegistryCache &cache, const RootMetadata &anchor,
                                        const QString &appVersion, const QStringList &channels, bool onlyMissing)
{
    QStringList wanted = channels;
    if (wanted.isEmpty())
        wanted = QDir(distributionDir + QStringLiteral("/channels")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QList<ChannelUpdate> results;
    const QStringList cached = cache.channels();
    for (const QString &channel : std::as_const(wanted)) {
        if (onlyMissing && cached.contains(channel))
            continue;
        auto imported = cache.importChannel(distributionDir, channel, anchor, appVersion);
        if (!imported.ok()) {
            results.append(failed(channel, imported.error(), distributionDir));
            continue;
        }
        ChannelUpdate update;
        update.channel = channel;
        update.status = ChannelUpdate::Status::Updated;
        update.sequence = imported.value().index.sequence;
        update.profileCount = imported.value().profiles.size();
        update.expired = imported.value().expired;
        update.mirror = distributionDir;
        results.append(update);
    }
    return results;
}

} // namespace saveforge::registry
