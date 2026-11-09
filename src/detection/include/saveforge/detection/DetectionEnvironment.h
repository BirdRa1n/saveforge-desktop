#pragma once

#include "saveforge/platform/FolderMap.h"
#include "saveforge/platform/Os.h"

#include <QHash>
#include <QString>
#include <QStringList>

#include <memory>

namespace saveforge::detection {

// Read-only access to the Windows registry. Keys use the full root name and
// backslashes, e.g. "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games".
class RegistryReader {
public:
    virtual ~RegistryReader() = default;
    [[nodiscard]] virtual QString value(const QString &key, const QString &name) const = 0;
    [[nodiscard]] virtual QStringList subkeys(const QString &key) const = 0;
};

// The host registry on Windows; an empty registry elsewhere.
[[nodiscard]] std::shared_ptr<const RegistryReader> hostRegistry();

// In-memory registry, for tests and for importing registry snapshots.
class MapRegistryReader final : public RegistryReader {
public:
    void set(const QString &key, const QString &name, const QString &value);
    [[nodiscard]] QString value(const QString &key, const QString &name) const override;
    [[nodiscard]] QStringList subkeys(const QString &key) const override;

private:
    QHash<QString, QHash<QString, QString>> m_values; // lower-cased key -> name -> value
};

// Everything detection reads from the machine. Detectors never query the
// system directly, so tests can describe any OS with fixture directories.
struct DetectionEnvironment {
    platform::Os hostOs = platform::hostOs();
    QString home;                                   // '/'-separated
    platform::FolderMap userFolders;                // root placeholders of the host user
    QString osUserName;
    std::shared_ptr<const RegistryReader> registry; // never null

    // The real machine running SaveForge.
    [[nodiscard]] static DetectionEnvironment host();
};

} // namespace saveforge::detection
