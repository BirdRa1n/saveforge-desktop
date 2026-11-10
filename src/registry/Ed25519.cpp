#include "savevault/registry/Ed25519.h"

#include <QCryptographicHash>

#include <monocypher-ed25519.h>

namespace savevault::registry {

bool verifyEd25519(QByteArrayView publicKey, QByteArrayView message, QByteArrayView signature)
{
    if (publicKey.size() != 32 || signature.size() != 64)
        return false;
    // crypto_ed25519_check returns 0 when the signature is valid.
    return crypto_ed25519_check(reinterpret_cast<const uint8_t *>(signature.data()),
                                reinterpret_cast<const uint8_t *>(publicKey.data()),
                                reinterpret_cast<const uint8_t *>(message.data()),
                                static_cast<size_t>(message.size()))
           == 0;
}

QString keyIdOf(QByteArrayView publicKey)
{
    return QString::fromLatin1(QCryptographicHash::hash(publicKey, QCryptographicHash::Sha256).toHex().left(16));
}

} // namespace savevault::registry
