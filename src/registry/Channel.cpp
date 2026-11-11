#include "savevault/registry/Channel.h"

#include "savevault/profiles/ProfileParser.h"
#include "savevault/registry/JsonSpans.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <zstd.h>

namespace savevault::registry {

namespace {

Result<QByteArray> readFile(const QString &path, qint64 maxBytes = 64LL * 1024 * 1024)
{
    QFile file(path);
    if (!file.exists())
        return Error{ErrorCode::NotFound, QStringLiteral("file not found"), path};
    if (!file.open(QIODevice::ReadOnly))
        return Error{ErrorCode::Io, file.errorString(), path};
    if (file.size() > maxBytes)
        return Error{ErrorCode::Corrupted, QStringLiteral("file is too large"), path};
    return file.readAll();
}

Error untrusted(const QString &message, const QString &context)
{
    return Error{ErrorCode::Untrusted, message, context};
}

// Reads `<path>` and `<path>.sig` and checks the signature threshold.
Result<QByteArray> readSigned(const QString &path, const QList<PublicKey> &keys, int threshold)
{
    auto bytes = readFile(path);
    if (!bytes.ok())
        return bytes.error();
    auto sigBytes = readFile(path + QStringLiteral(".sig"));
    if (!sigBytes.ok())
        return sigBytes.error();
    auto signatures = parseSignatures(sigBytes.value());
    if (!signatures.ok())
        return signatures.error();
    if (countValidSignatures(bytes.value(), signatures.value(), keys) < threshold)
        return untrusted(QStringLiteral("signature verification failed"), path);
    return bytes;
}

Result<QByteArray> decompress(const QByteArray &compressed, qint64 expectedSize, const QString &context)
{
    const unsigned long long declared = ZSTD_getFrameContentSize(compressed.constData(), static_cast<size_t>(compressed.size()));
    if (declared == ZSTD_CONTENTSIZE_ERROR || declared == ZSTD_CONTENTSIZE_UNKNOWN
        || declared != static_cast<unsigned long long>(expectedSize))
        return Error{ErrorCode::Corrupted, QStringLiteral("bundle size does not match the index"), context};
    QByteArray out(static_cast<qsizetype>(expectedSize), Qt::Uninitialized);
    const size_t written = ZSTD_decompress(out.data(), static_cast<size_t>(out.size()), compressed.constData(),
                                           static_cast<size_t>(compressed.size()));
    if (ZSTD_isError(written) || written != static_cast<size_t>(expectedSize))
        return Error{ErrorCode::Corrupted, QStringLiteral("bundle cannot be decompressed"), context};
    return out;
}

bool isRevoked(const profiles::Profile &profile, const QList<RevokedProfile> &revoked)
{
    return std::any_of(revoked.begin(), revoked.end(), [&profile](const RevokedProfile &r) {
        return r.id == profile.id && r.revision == profile.revision;
    });
}

} // namespace

Result<RootMetadata> updateRootChain(const QString &distributionDir, const RootMetadata &trusted)
{
    RootMetadata current = trusted;
    // The distribution's copy of the anchor's own version may be fresher (e.g. a
    // later expiry); use it when it is signed by the anchor's root keys.
    const QString samePath = distributionDir + QStringLiteral("/roots/%1.json").arg(current.version);
    if (QFile::exists(samePath)) {
        auto bytes = readSigned(samePath, current.rootKeys, current.rootThreshold);
        if (!bytes.ok())
            return bytes.error();
        auto root = parseRoot(bytes.value());
        if (!root.ok())
            return root.error();
        if (root.value().version != current.version)
            return untrusted(QStringLiteral("root version mismatch"), samePath);
        current = std::move(root).value();
    }
    for (;;) {
        const int next = current.version + 1;
        const QString path = distributionDir + QStringLiteral("/roots/%1.json").arg(next);
        if (!QFile::exists(path))
            return current;
        auto bytes = readSigned(path, current.rootKeys, current.rootThreshold);
        if (!bytes.ok())
            return bytes.error().code == ErrorCode::Untrusted
                       ? untrusted(QStringLiteral("root version %1 is not signed by the previous root keys").arg(next), path)
                       : bytes.error();
        auto root = parseRoot(bytes.value());
        if (!root.ok())
            return root.error();
        if (root.value().version != next)
            return untrusted(QStringLiteral("root version mismatch"), path);
        current = std::move(root).value();
    }
}

Result<VerifiedChannel> loadChannel(const QString &distributionDir, const QString &channel,
                                    const RootMetadata &trustAnchor, const LoadOptions &options)
{
    VerifiedChannel result;
    result.channel = channel;

    auto root = updateRootChain(distributionDir, trustAnchor);
    if (!root.ok())
        return root.error();
    result.root = std::move(root).value();

    // Index: signed by an online key allowed for this channel.
    QList<PublicKey> onlineKeys;
    for (const PublicKey &key : result.root.onlineKeys) {
        if (key.channels.contains(QStringLiteral("*")) || key.channels.contains(channel))
            onlineKeys.append(key);
    }
    const QString channelDir = distributionDir + QStringLiteral("/channels/") + channel;
    auto indexBytes = readSigned(channelDir + QStringLiteral("/index.json"), onlineKeys, 1);
    if (!indexBytes.ok())
        return indexBytes.error();
    auto index = parseIndex(indexBytes.value());
    if (!index.ok())
        return index.error();
    result.index = std::move(index).value();

    if (result.index.channel != channel)
        return untrusted(QStringLiteral("index belongs to channel '%1'").arg(result.index.channel), channelDir);
    if (result.index.sequence < options.minimumSequence)
        return untrusted(QStringLiteral("index sequence %1 is older than the accepted %2 (rollback)")
                             .arg(result.index.sequence)
                             .arg(options.minimumSequence),
                         channelDir);
    if (!result.index.minAppVersion.isEmpty() && !options.appVersion.isEmpty()
        && compareVersions(options.appVersion, result.index.minAppVersion) < 0)
        return Error{ErrorCode::Unsupported,
                     QStringLiteral("this channel requires SaveVault %1 or newer").arg(result.index.minAppVersion), channel};
    result.expired = result.index.expires < options.now;

    // Bundle: size and hash from the signed index, then bounded decompression.
    const QString bundlePath = channelDir + u'/' + result.index.bundlePath;
    QByteArray text;
    {
        auto compressed = readFile(bundlePath, result.index.bundleSize);
        if (!compressed.ok())
            return compressed.error();
        if (compressed.value().size() != result.index.bundleSize
            || QString::fromLatin1(QCryptographicHash::hash(compressed.value(), QCryptographicHash::Sha256).toHex())
                   != result.index.bundleSha256)
            return untrusted(QStringLiteral("bundle does not match the signed index"), bundlePath);
        auto bundleBytes = decompress(compressed.value(), result.index.uncompressedSize, bundlePath);
        if (!bundleBytes.ok())
            return bundleBytes.error();
        text = std::move(bundleBytes).value();
    }

    // The bundle is read one value at a time (JsonSpans): a QJsonDocument of
    // the whole text would cost about eight times its size, over 100 MB for
    // the Ludusavi channel. Each value is still fully validated by Qt's parser.
    const JsonSpans spans(text);
    const auto corrupted = [&bundlePath](const QString &what) {
        return Error{ErrorCode::Corrupted, what, bundlePath};
    };
    const auto bundleRoot = spans.root();
    const auto members = bundleRoot ? spans.objectMembers(*bundleRoot) : std::nullopt;
    if (!members)
        return corrupted(QStringLiteral("bundle is not a JSON object"));
    const auto member = [&](const char *key) -> QJsonValue {
        const auto it = members->constFind(QLatin1String(key));
        if (it == members->cend())
            return QJsonValue(QJsonValue::Undefined);
        return spans.parse(*it).value_or(QJsonValue(QJsonValue::Undefined));
    };
    if (member("schemaVersion").toInt() != 1 || member("type").toString() != u"bundle"
        || member("channel").toString() != channel || member("sequence").toInteger() != result.index.sequence)
        return corrupted(QStringLiteral("bundle header does not match the index"));

    for (const QJsonValue &value : member("notices").toArray()) {
        const QJsonObject notice = value.toObject();
        result.notices.append({notice.value(QLatin1String("license")).toString(),
                               notice.value(QLatin1String("attribution")).toString(),
                               notice.value(QLatin1String("url")).toString()});
    }
    if (result.notices.isEmpty())
        return corrupted(QStringLiteral("bundle has no license notice"));

    const auto profilesSpan = members->constFind(QStringLiteral("profiles"));
    const auto profileSpans = profilesSpan == members->cend() ? std::nullopt : spans.arrayElements(*profilesSpan);
    if (!profileSpans)
        return corrupted(QStringLiteral("bundle has no profile list"));
    result.profiles.reserve(profileSpans->size());
    QSet<QString> ids;
    for (const JsonSpan &span : *profileSpans) {
        const auto parsed = spans.parse(span);
        if (!parsed || !parsed->isObject())
            return corrupted(QStringLiteral("bundle has a profile that is not valid JSON"));
        const QJsonObject json = parsed->toObject();
        const QString id = json.value(QLatin1String("id")).toString();
        auto profile = profiles::parseProfile(json);
        if (!profile.ok()) {
            result.rejectedProfiles.append(id + QStringLiteral(": ") + profile.error().toString());
            continue;
        }
        if (ids.contains(id)) {
            result.rejectedProfiles.append(id + QStringLiteral(": duplicate id"));
            continue;
        }
        if (isRevoked(profile.value(), result.index.revoked)) {
            result.rejectedProfiles.append(id + QStringLiteral(": revoked"));
            continue;
        }
        ids.insert(id);
        result.profiles.append(std::move(profile).value());
    }
    return result;
}

} // namespace savevault::registry
