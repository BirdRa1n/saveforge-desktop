#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

namespace savevault::registry {

// Ed25519 (RFC 8032) signature verification, backed by Monocypher.
// `publicKey` is the raw 32-byte key, `signature` the 64-byte signature.
// Returns false for malformed inputs instead of throwing.
[[nodiscard]] bool verifyEd25519(QByteArrayView publicKey, QByteArrayView message, QByteArrayView signature);

// Registry key id: first 16 lower-case hex characters of SHA-256(publicKey)
// (savevault-specs/registry/SPEC.md section 2).
[[nodiscard]] QString keyIdOf(QByteArrayView publicKey);

} // namespace savevault::registry
