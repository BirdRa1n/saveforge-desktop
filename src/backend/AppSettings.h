#pragma once

#include <QString>
#include <QUuid>

namespace saveforge::backend {

// User settings persisted in <app data>/settings.ini until the local database
// (roadmap task D1.2) takes over.
class AppSettings {
public:
    AppSettings();

    [[nodiscard]] QString dataDirectory() const { return m_dataDir; }
    [[nodiscard]] QString registryCacheDirectory() const;

    // Default: <app data>/vault.
    [[nodiscard]] QString vaultPath() const;
    void setVaultPath(const QString &path);

    // Random id of this installation, created on first use. Recorded in
    // snapshots to tell machines apart; never derived from hardware ids.
    [[nodiscard]] QUuid installationId();

    // "ask", "prefer-launcher" or "prefer-saveforge".
    [[nodiscard]] QString cloudPolicy() const;
    void setCloudPolicy(const QString &policy);

private:
    [[nodiscard]] QString file() const;
    QString m_dataDir;
};

} // namespace saveforge::backend
