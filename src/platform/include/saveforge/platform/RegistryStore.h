#pragma once

#include "saveforge/core/Result.h"

#include <QHash>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QString>

#include <memory>

namespace saveforge::platform {

// One registry value as the vault spec stores it (vault/SPEC.md section 3):
// REG_SZ and REG_EXPAND_SZ as strings, REG_MULTI_SZ as an array of strings,
// REG_DWORD as an integer, REG_QWORD as a decimal string and REG_BINARY as
// base64. An empty name is the key's default value.
struct RegistryValueData {
    QString name;
    QString type;
    QJsonValue data;
};

struct RegistryKeyData {
    QString key;                       // "HKCU/Software/Studio/Game/Sub"
    QList<RegistryValueData> values;
};

// The current user's registry hive, for registry save rules (profile/SPEC.md
// section 5). Only keys under HKCU/Software are accepted.
class RegistryStore {
public:
    virtual ~RegistryStore() = default;

    // False where there is no registry to use (outside Windows).
    [[nodiscard]] virtual bool available() const = 0;
    // The key and every subkey below it, parents first, with their values.
    // An empty list when the key does not exist. Values of other types
    // (REG_NONE, REG_LINK, ...) are skipped.
    [[nodiscard]] virtual Result<QList<RegistryKeyData>> readTree(const QString &key) const = 0;
    // Creates the key when missing and sets the given values; other values of
    // the key stay as they are.
    [[nodiscard]] virtual Result<void> writeValues(const RegistryKeyData &key) = 0;
    // Deletes the key and everything below it (no error when it is missing).
    [[nodiscard]] virtual Result<void> removeTree(const QString &key) = 0;
};

// Whether `key` names a key below HKCU/Software (case-insensitive), without
// empty, "." or ".." segments.
[[nodiscard]] bool isUserSoftwareKey(const QString &key);

// HKEY_CURRENT_USER on Windows; an unavailable store elsewhere.
[[nodiscard]] std::shared_ptr<RegistryStore> hostRegistryStore();

// In-memory registry with the same rules, for tests.
class MemoryRegistryStore final : public RegistryStore {
public:
    [[nodiscard]] bool available() const override { return true; }
    [[nodiscard]] Result<QList<RegistryKeyData>> readTree(const QString &key) const override;
    [[nodiscard]] Result<void> writeValues(const RegistryKeyData &key) override;
    [[nodiscard]] Result<void> removeTree(const QString &key) override;

private:
    // lower-cased key -> (original key, values by name)
    QMap<QString, RegistryKeyData> m_keys;
};

} // namespace saveforge::platform
