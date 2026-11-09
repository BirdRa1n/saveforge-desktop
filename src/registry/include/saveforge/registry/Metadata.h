#pragma once

#include "saveforge/core/Result.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace saveforge::registry {

// Registry metadata documents (saveforge-specs/registry, schemas root.v1,
// index.v1 and signature.v1), parsed strictly.

struct PublicKey {
    QString keyId;
    QByteArray key;          // raw 32 bytes
    QStringList channels;    // online keys only; "*" = every channel
};

struct RootMetadata {
    int version = 1;
    QDateTime expires;
    int rootThreshold = 1;
    QList<PublicKey> rootKeys;
    QList<PublicKey> onlineKeys;
};

struct RevokedProfile {
    QString id;
    int revision = 0;
    QString reason;
};

struct ChannelIndex {
    QString channel;
    qint64 sequence = 0;
    QDateTime publishedAt;
    QDateTime expires;
    QString minAppVersion;
    QString bundlePath;
    QString bundleSha256;
    qint64 bundleSize = 0;
    qint64 uncompressedSize = 0;
    qint64 profileCount = 0;
    QList<RevokedProfile> revoked;
};

struct Signature {
    QString keyId;
    QByteArray signature;    // raw 64 bytes
};

[[nodiscard]] Result<RootMetadata> parseRoot(const QByteArray &json);
[[nodiscard]] Result<ChannelIndex> parseIndex(const QByteArray &json);
[[nodiscard]] Result<QList<Signature>> parseSignatures(const QByteArray &json);

// Number of distinct `keys` with a valid signature over `bytes`.
[[nodiscard]] int countValidSignatures(const QByteArray &bytes, const QList<Signature> &signatures,
                                       const QList<PublicKey> &keys);

// -1, 0 or 1 comparing MAJOR.MINOR.PATCH (pre-release suffixes are ignored).
[[nodiscard]] int compareVersions(const QString &a, const QString &b);

} // namespace saveforge::registry
