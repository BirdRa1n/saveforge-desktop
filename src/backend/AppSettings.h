#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUuid>

namespace savevault::backend {

// User settings persisted in <app data>/settings.ini until the local database
// (roadmap task D1.2) takes over.
class AppSettings {
public:
    AppSettings();

    [[nodiscard]] QString dataDirectory() const { return m_dataDir; }
    [[nodiscard]] QString registryCacheDirectory() const;
    // Local profiles and games added by hand (services::LocalProfileStore).
    [[nodiscard]] QString localProfilesDirectory() const;

    // Default: <app data>/vault.
    [[nodiscard]] QString vaultPath() const;
    void setVaultPath(const QString &path);

    // Random id of this installation, created on first use. Recorded in
    // snapshots to tell machines apart; never derived from hardware ids.
    [[nodiscard]] QUuid installationId();

    // "ask", "prefer-launcher" or "prefer-savevault".
    [[nodiscard]] QString cloudPolicy() const;
    void setCloudPolicy(const QString &policy);

    // Registry mirrors tried in order. Default: the mirrors of this build.
    [[nodiscard]] QStringList registryMirrors() const;
    // Whether the app checks the mirrors for new profiles once a day.
    [[nodiscard]] bool autoUpdateRegistry() const;
    void setAutoUpdateRegistry(bool enabled);
    // When the mirrors last answered a check (invalid if never).
    [[nodiscard]] QDateTime lastRegistryCheck() const;
    void setLastRegistryCheck(const QDateTime &when);

    // Save slot whose files are on disk for a game (vault snapshot `slot`);
    // empty for the unnamed progress. Local to this machine.
    [[nodiscard]] QString activeSlot(const QString &gameId) const;
    void setActiveSlot(const QString &gameId, const QString &slot);

    // Automatic backups: when a game closes, and every N hours (0 = off).
    [[nodiscard]] bool backupOnGameExit() const;
    void setBackupOnGameExit(bool enabled);
    [[nodiscard]] int backupIntervalHours() const;
    void setBackupIntervalHours(int hours);
    [[nodiscard]] QDateTime lastScheduledBackup() const;
    void setLastScheduledBackup(const QDateTime &when);
    // Whether closing the window keeps the app running in the system tray,
    // so automatic backups keep working.
    [[nodiscard]] bool runInBackground() const;
    void setRunInBackground(bool enabled);

    // Retention: keep the newest N backups and/or the last N days (0 = no limit).
    [[nodiscard]] int retentionKeepLast() const;
    void setRetentionKeepLast(int count);
    [[nodiscard]] int retentionKeepDays() const;
    void setRetentionKeepDays(int days);
    // When the vault was last verified and cleaned (weekly maintenance).
    [[nodiscard]] QDateTime lastMaintenance() const;
    void setLastMaintenance(const QDateTime &when);

    // What earlier .gamesave exports carried (services::ExportLog).
    [[nodiscard]] QString exportLogDirectory() const;

    // Covers downloaded for games without local art.
    [[nodiscard]] QString coverCacheDirectory() const;
    // Whether covers missing on this machine may be downloaded from Steam's CDN.
    [[nodiscard]] bool downloadCovers() const;
    void setDownloadCovers(bool enabled);

    // Appearance: "system", "dark" or "light", and the accent colour as #RRGGBB.
    [[nodiscard]] QString themeMode() const;
    void setThemeMode(const QString &mode);
    [[nodiscard]] QString accentColor() const;
    void setAccentColor(const QString &color);

private:
    [[nodiscard]] QString file() const;
    QString m_dataDir;
};

} // namespace savevault::backend
