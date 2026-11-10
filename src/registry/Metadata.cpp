#include "savevault/registry/Metadata.h"

#include "savevault/core/JsonReader.h"
#include "savevault/registry/Ed25519.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <limits>

namespace savevault::registry {

namespace {

QString child(const QString &p, const QString &k) { return JsonReader::child(p, k); }
QString child(const QString &p, qsizetype i) { return JsonReader::child(p, i); }

const QRegularExpression &keyIdPattern()
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{16}$"));
    return re;
}

const QRegularExpression &slugPattern()
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9]+(-[a-z0-9]+)*$"));
    return re;
}

Result<QJsonObject> parseObject(const QByteArray &json, const char *what)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return Error{ErrorCode::Corrupted, QStringLiteral("%1 is not a JSON object").arg(QLatin1String(what)),
                     error.errorString()};
    return doc.object();
}

bool readTimestamp(JsonReader &r, const QJsonValue &value, const QString &pointer, QDateTime &out)
{
    QString text;
    if (!r.string(value, pointer, text, 20, 40))
        return false;
    out = QDateTime::fromString(text, Qt::ISODateWithMs);
    return out.isValid() || r.fail(pointer, QStringLiteral("must be an RFC 3339 date-time"));
}

bool readPublicKey(JsonReader &r, const QJsonValue &value, const QString &ptr, bool online, PublicKey &out)
{
    QJsonObject o;
    if (!r.object(value, ptr, o))
        return false;
    if (online)
        r.keys(o, ptr, {"keyId", "algorithm", "publicKey", "channels"}, {"keyId", "algorithm", "publicKey", "channels"});
    else
        r.keys(o, ptr, {"keyId", "algorithm", "publicKey"}, {"keyId", "algorithm", "publicKey"});
    QString algorithm;
    QString key;
    r.string(o.value(QLatin1String("keyId")), child(ptr, QStringLiteral("keyId")), out.keyId, 16, 16, &keyIdPattern());
    r.enumeration(o.value(QLatin1String("algorithm")), child(ptr, QStringLiteral("algorithm")), algorithm,
                  {QStringLiteral("ed25519")});
    if (r.string(o.value(QLatin1String("publicKey")), child(ptr, QStringLiteral("publicKey")), key, 44, 44)) {
        out.key = QByteArray::fromBase64(key.toLatin1());
        if (out.key.size() != 32)
            r.fail(child(ptr, QStringLiteral("publicKey")), QStringLiteral("must be 32 bytes"));
        else if (keyIdOf(out.key) != out.keyId)
            r.fail(child(ptr, QStringLiteral("keyId")), QStringLiteral("does not match the public key"));
    }
    if (online) {
        static const QRegularExpression channel(QStringLiteral("^(\\*|[a-z0-9]+(-[a-z0-9]+)*)$"));
        r.stringList(o.value(QLatin1String("channels")), child(ptr, QStringLiteral("channels")), out.channels, 1,
                     JsonReader::kUnbounded, 1, 100, &channel);
    }
    return !r.failed();
}

int majorMinorPatch(const QString &version, int part)
{
    return version.section(u'-', 0, 0).section(u'.', part, part).toInt();
}

} // namespace

int compareVersions(const QString &a, const QString &b)
{
    for (int part = 0; part < 3; ++part) {
        const int x = majorMinorPatch(a, part);
        const int y = majorMinorPatch(b, part);
        if (x != y)
            return x < y ? -1 : 1;
    }
    return 0;
}

Result<RootMetadata> parseRoot(const QByteArray &json)
{
    auto parsed = parseObject(json, "root.json");
    if (!parsed.ok())
        return parsed.error();
    const QJsonObject o = parsed.value();
    JsonReader r;
    RootMetadata root;
    r.keys(o, QString(), {"schemaVersion", "type", "version", "expires", "rootThreshold", "rootKeys", "onlineKeys"},
           {"schemaVersion", "type", "version", "expires", "rootThreshold", "rootKeys", "onlineKeys"});
    qint64 number = 0;
    if (r.integer(o.value(QLatin1String("schemaVersion")), QStringLiteral("/schemaVersion"), number, 1) && number != 1)
        return Error{ErrorCode::Unsupported, QStringLiteral("root schemaVersion %1 is not supported").arg(number), {}};
    QString type;
    r.enumeration(o.value(QLatin1String("type")), QStringLiteral("/type"), type, {QStringLiteral("root")});
    if (r.integer(o.value(QLatin1String("version")), QStringLiteral("/version"), number, 1, std::numeric_limits<int>::max()))
        root.version = static_cast<int>(number);
    readTimestamp(r, o.value(QLatin1String("expires")), QStringLiteral("/expires"), root.expires);
    if (r.integer(o.value(QLatin1String("rootThreshold")), QStringLiteral("/rootThreshold"), number, 1, 100))
        root.rootThreshold = static_cast<int>(number);

    for (const auto &[name, online, list] : {std::tuple{"rootKeys", false, &root.rootKeys},
                                             std::tuple{"onlineKeys", true, &root.onlineKeys}}) {
        QJsonArray keys;
        const QString ptr = u'/' + QLatin1String(name);
        if (!r.array(o.value(QLatin1String(name)), ptr, keys, 1))
            break;
        for (qsizetype i = 0; i < keys.size(); ++i) {
            PublicKey key;
            if (readPublicKey(r, keys.at(i), child(ptr, i), online, key))
                list->append(key);
        }
    }
    if (!r.failed() && root.rootThreshold > root.rootKeys.size())
        r.fail(QStringLiteral("/rootThreshold"), QStringLiteral("exceeds the number of root keys"));
    if (r.failed())
        return r.error();
    return root;
}

Result<ChannelIndex> parseIndex(const QByteArray &json)
{
    auto parsed = parseObject(json, "index.json");
    if (!parsed.ok())
        return parsed.error();
    const QJsonObject o = parsed.value();
    JsonReader r;
    ChannelIndex index;
    r.keys(o, QString(),
           {"schemaVersion", "type", "channel", "sequence", "publishedAt", "expires", "minAppVersion", "bundle", "revoked",
            "source"},
           {"schemaVersion", "type", "channel", "sequence", "publishedAt", "expires", "bundle"});
    qint64 number = 0;
    if (r.integer(o.value(QLatin1String("schemaVersion")), QStringLiteral("/schemaVersion"), number, 1) && number != 1)
        return Error{ErrorCode::Unsupported, QStringLiteral("index schemaVersion %1 is not supported").arg(number), {}};
    QString type;
    r.enumeration(o.value(QLatin1String("type")), QStringLiteral("/type"), type, {QStringLiteral("index")});
    r.string(o.value(QLatin1String("channel")), QStringLiteral("/channel"), index.channel, 1, 100, &slugPattern());
    r.integer(o.value(QLatin1String("sequence")), QStringLiteral("/sequence"), index.sequence, 1);
    readTimestamp(r, o.value(QLatin1String("publishedAt")), QStringLiteral("/publishedAt"), index.publishedAt);
    readTimestamp(r, o.value(QLatin1String("expires")), QStringLiteral("/expires"), index.expires);
    if (o.contains(QLatin1String("minAppVersion"))) {
        static const QRegularExpression semver(
            QStringLiteral("^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)(-[0-9A-Za-z.-]+)?$"));
        r.string(o.value(QLatin1String("minAppVersion")), QStringLiteral("/minAppVersion"), index.minAppVersion, 5, 64, &semver);
    }

    QJsonObject bundle;
    if (r.object(o.value(QLatin1String("bundle")), QStringLiteral("/bundle"), bundle)
        && r.keys(bundle, QStringLiteral("/bundle"), {"path", "sha256", "size", "uncompressedSize", "profileCount"},
                  {"path", "sha256", "size", "uncompressedSize", "profileCount"})) {
        static const QRegularExpression path(QStringLiteral("^bundles/[a-z0-9][a-z0-9.-]*\\.json\\.zst$"));
        static const QRegularExpression sha(QStringLiteral("^[0-9a-f]{64}$"));
        r.string(bundle.value(QLatin1String("path")), QStringLiteral("/bundle/path"), index.bundlePath, 1, 200, &path);
        r.string(bundle.value(QLatin1String("sha256")), QStringLiteral("/bundle/sha256"), index.bundleSha256, 64, 64, &sha);
        r.integer(bundle.value(QLatin1String("size")), QStringLiteral("/bundle/size"), index.bundleSize, 1);
        r.integer(bundle.value(QLatin1String("uncompressedSize")), QStringLiteral("/bundle/uncompressedSize"),
                  index.uncompressedSize, 1, 64LL * 1024 * 1024);
        r.integer(bundle.value(QLatin1String("profileCount")), QStringLiteral("/bundle/profileCount"), index.profileCount, 0);
    }

    if (o.contains(QLatin1String("revoked"))) {
        QJsonArray revoked;
        if (r.array(o.value(QLatin1String("revoked")), QStringLiteral("/revoked"), revoked)) {
            for (qsizetype i = 0; i < revoked.size() && !r.failed(); ++i) {
                const QString ptr = child(QStringLiteral("/revoked"), i);
                QJsonObject entry;
                if (!r.object(revoked.at(i), ptr, entry) || !r.keys(entry, ptr, {"id", "revision", "reason"}, {"id", "revision", "reason"}))
                    break;
                RevokedProfile item;
                qint64 revision = 0;
                r.string(entry.value(QLatin1String("id")), child(ptr, QStringLiteral("id")), item.id, 1, 100, &slugPattern());
                if (r.integer(entry.value(QLatin1String("revision")), child(ptr, QStringLiteral("revision")), revision, 1,
                              std::numeric_limits<int>::max()))
                    item.revision = static_cast<int>(revision);
                r.string(entry.value(QLatin1String("reason")), child(ptr, QStringLiteral("reason")), item.reason, 1, 500);
                index.revoked.append(item);
            }
        }
    }
    // `source` is informative only; its presence is enough.
    if (r.failed())
        return r.error();
    return index;
}

Result<QList<Signature>> parseSignatures(const QByteArray &json)
{
    auto parsed = parseObject(json, "signature file");
    if (!parsed.ok())
        return parsed.error();
    JsonReader r;
    QList<Signature> signatures;
    r.keys(parsed.value(), QString(), {"signatures"}, {"signatures"});
    QJsonArray items;
    if (r.array(parsed.value().value(QLatin1String("signatures")), QStringLiteral("/signatures"), items, 1, 8)) {
        for (qsizetype i = 0; i < items.size() && !r.failed(); ++i) {
            const QString ptr = child(QStringLiteral("/signatures"), i);
            QJsonObject entry;
            if (!r.object(items.at(i), ptr, entry) || !r.keys(entry, ptr, {"keyId", "signature"}, {"keyId", "signature"}))
                break;
            Signature signature;
            QString encoded;
            r.string(entry.value(QLatin1String("keyId")), child(ptr, QStringLiteral("keyId")), signature.keyId, 16, 16,
                     &keyIdPattern());
            if (r.string(entry.value(QLatin1String("signature")), child(ptr, QStringLiteral("signature")), encoded, 88, 88)) {
                signature.signature = QByteArray::fromBase64(encoded.toLatin1());
                if (signature.signature.size() != 64)
                    r.fail(child(ptr, QStringLiteral("signature")), QStringLiteral("must be 64 bytes"));
            }
            signatures.append(signature);
        }
    }
    if (r.failed())
        return r.error();
    return signatures;
}

int countValidSignatures(const QByteArray &bytes, const QList<Signature> &signatures, const QList<PublicKey> &keys)
{
    QSet<QString> valid;
    for (const Signature &signature : signatures) {
        for (const PublicKey &key : keys) {
            if (key.keyId == signature.keyId && verifyEd25519(key.key, bytes, signature.signature))
                valid.insert(key.keyId);
        }
    }
    return static_cast<int>(valid.size());
}

} // namespace savevault::registry
