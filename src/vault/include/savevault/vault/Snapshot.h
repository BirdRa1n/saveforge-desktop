#pragma once

#include "savevault/core/Result.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QUuid>

#include <optional>

namespace savevault::vault {

// In-memory form of a snapshot document (savevault-specs/vault, snapshot.v1).
// A snapshot is the immutable record of one backup of one game.

enum class SnapshotKind { Manual, Auto, PreRestore, Import };
enum class SnapshotTrigger { GameExit, Schedule, FileChange };
enum class Consistency { Consistent, Inconsistent };

struct SnapshotFile {
    QString path;                          // concrete path in template form
    QHash<QString, QString> bindings;      // segment placeholder values on the source machine
    QString ruleId;
    QString sha256;                        // lower-case hex of the original content
    qint64 size = 0;
    QDateTime mtime;                       // UTC
};

struct RegistryValue {
    QString name;                          // empty: the key's default value
    QString type;                          // REG_SZ, REG_EXPAND_SZ, REG_MULTI_SZ, REG_DWORD, REG_QWORD, REG_BINARY
    QJsonValue data;                       // representation defined by the spec for each type
};

struct RegistryKey {
    QString key;                           // "HKCU/Software/..."
    QString ruleId;
    QList<RegistryValue> values;
};

struct ProfileRef {
    QString channel;                       // official, upstream-ludusavi, community-experimental, local
    QString id;
    int revision = 1;
};

struct Snapshot {
    QUuid snapshotId;
    std::optional<QUuid> parentId;         // previous snapshot of the game on this machine
    QString gameId;                        // profile id or "local:<uuid>"
    QDateTime createdAt;                   // UTC
    SnapshotKind kind = SnapshotKind::Manual;
    std::optional<SnapshotTrigger> trigger;
    QString label;
    QString slot;                          // save slot the files belong to; empty: unnamed progress
    bool pinned = false;
    QUuid machineId;
    QString appVersion;
    ProfileRef profile;
    Consistency consistency = Consistency::Consistent;
    QList<SnapshotFile> files;
    QList<RegistryKey> registry;
};

// Content of vault.json (vault.v1).
struct VaultDescriptor {
    QUuid vaultId;
    QDateTime createdAt;                   // UTC
};

// Serialization. Output is UTF-8 JSON with LF line endings.
[[nodiscard]] QByteArray toJson(const Snapshot &snapshot);
[[nodiscard]] QByteArray toJson(const VaultDescriptor &descriptor);

// Strict parsing: every constraint of the schema plus path-safety rules P1-P4
// on file paths, registry-key rules and bindings for every segment placeholder.
[[nodiscard]] Result<Snapshot> parseSnapshot(const QByteArray &json);
[[nodiscard]] Result<Snapshot> parseSnapshot(const QJsonObject &json);
[[nodiscard]] Result<VaultDescriptor> parseVaultDescriptor(const QByteArray &json);

// Directory name of a game inside snapshots/: "local:<uuid>" becomes
// "local_<uuid>" because ':' is not valid in Windows file names.
[[nodiscard]] QString gameDirectoryName(const QString &gameId);

// Whether `name` can name a save slot (1-100 characters, no control
// characters, no leading or trailing whitespace; vault/SPEC.md section 3).
[[nodiscard]] bool isValidSlotName(const QString &name);

} // namespace savevault::vault
