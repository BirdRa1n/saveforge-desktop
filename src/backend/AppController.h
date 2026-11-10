#pragma once

#include "AppSettings.h"
#include "LibraryModel.h"

#include "saveforge/services/AutoBackup.h"
#include "saveforge/services/Library.h"
#include "saveforge/services/Retention.h"
#include "saveforge/services/RestoreService.h"
#include "saveforge/vault/Vault.h"

#include <QFutureWatcher>
#include <QTimer>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <optional>

namespace saveforge::backend {

// Entry point of the QML interface (singleton `AppController`).
//
// Owns the settings, the vault and the library. Long operations (scanning the
// machine, backups, restores, registry imports) run one at a time on a worker
// thread; `busy` and `busyText` describe the running one, and every outcome is
// appended to `activity`. Methods that only read (snapshots, restore plans)
// answer synchronously with plain QVariant maps for QML.
class AppController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(saveforge::backend::LibraryModel *library READ library CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString busyText READ busyText NOTIFY busyChanged)
    Q_PROPERTY(int profileCount READ profileCount NOTIFY catalogChanged)
    Q_PROPERTY(bool usingTestKeys READ usingTestKeys NOTIFY catalogChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY catalogChanged)
    Q_PROPERTY(QStringList notices READ notices NOTIFY catalogChanged)
    Q_PROPERTY(QString vaultPath READ vaultPath NOTIFY vaultChanged)
    Q_PROPERTY(bool vaultReady READ vaultReady NOTIFY vaultChanged)
    Q_PROPERTY(QString vaultError READ vaultError NOTIFY vaultChanged)
    Q_PROPERTY(QString cloudPolicy READ cloudPolicy WRITE setCloudPolicy NOTIFY cloudPolicyChanged)
    Q_PROPERTY(QVariantList activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(bool canUpdateRegistry READ canUpdateRegistry CONSTANT)
    Q_PROPERTY(bool autoUpdateRegistry READ autoUpdateRegistry WRITE setAutoUpdateRegistry NOTIFY registryChanged)
    Q_PROPERTY(QString registryCheckedText READ registryCheckedText NOTIFY registryChanged)
    Q_PROPERTY(bool downloadCovers READ downloadCovers WRITE setDownloadCovers NOTIFY coversChanged)
    Q_PROPERTY(bool backupOnGameExit READ backupOnGameExit WRITE setBackupOnGameExit NOTIFY autoBackupChanged)
    Q_PROPERTY(int backupIntervalHours READ backupIntervalHours WRITE setBackupIntervalHours NOTIFY autoBackupChanged)
    Q_PROPERTY(bool runInBackground READ runInBackground WRITE setRunInBackground NOTIFY autoBackupChanged)
    Q_PROPERTY(int retentionKeepLast READ retentionKeepLast WRITE setRetentionKeepLast NOTIFY retentionChanged)
    Q_PROPERTY(int retentionKeepDays READ retentionKeepDays WRITE setRetentionKeepDays NOTIFY retentionChanged)
    Q_PROPERTY(QString maintenanceText READ maintenanceText NOTIFY retentionChanged)
    // Set by the executable when it shows a system tray icon.
    Q_PROPERTY(bool trayAvailable READ trayAvailable WRITE setTrayAvailable NOTIFY trayAvailableChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    [[nodiscard]] LibraryModel *library() { return &m_library; }
    [[nodiscard]] bool busy() const { return !m_busyText.isEmpty(); }
    [[nodiscard]] QString busyText() const { return m_busyText; }
    [[nodiscard]] int profileCount() const { return m_profileCount; }
    [[nodiscard]] bool usingTestKeys() const { return m_usingTestKeys; }
    [[nodiscard]] QStringList warnings() const { return m_warnings; }
    [[nodiscard]] QStringList notices() const { return m_notices; }
    [[nodiscard]] QString vaultPath() const { return m_settings.vaultPath(); }
    [[nodiscard]] bool vaultReady() const { return m_vault.has_value(); }
    [[nodiscard]] QString vaultError() const { return m_vaultError; }
    [[nodiscard]] QString cloudPolicy() const { return m_settings.cloudPolicy(); }
    void setCloudPolicy(const QString &policy);
    [[nodiscard]] QVariantList activity() const { return m_activity; }
    // True when this build or the settings name at least one registry mirror.
    [[nodiscard]] bool canUpdateRegistry() const { return !m_settings.registryMirrors().isEmpty(); }
    [[nodiscard]] bool autoUpdateRegistry() const { return m_settings.autoUpdateRegistry(); }
    [[nodiscard]] bool downloadCovers() const { return m_settings.downloadCovers(); }
    void setDownloadCovers(bool enabled);
    [[nodiscard]] bool backupOnGameExit() const { return m_settings.backupOnGameExit(); }
    void setBackupOnGameExit(bool enabled);
    [[nodiscard]] int backupIntervalHours() const { return m_settings.backupIntervalHours(); }
    void setBackupIntervalHours(int hours);
    [[nodiscard]] bool runInBackground() const { return m_settings.runInBackground(); }
    void setRunInBackground(bool enabled);
    [[nodiscard]] int retentionKeepLast() const { return m_settings.retentionKeepLast(); }
    void setRetentionKeepLast(int count);
    [[nodiscard]] int retentionKeepDays() const { return m_settings.retentionKeepDays(); }
    void setRetentionKeepDays(int days);
    // "Última verificação: há 3 dias." for the settings page.
    [[nodiscard]] QString maintenanceText() const;
    [[nodiscard]] bool trayAvailable() const { return m_trayAvailable; }
    void setTrayAvailable(bool available);

    // Checks for games that closed and for the backup schedule now (it also
    // runs on a timer). Exposed for tests.
    Q_INVOKABLE void checkAutomaticBackups();
    void setAutoUpdateRegistry(bool enabled);
    // "Verificado há 2 horas", "Ainda não verificado", for the settings page.
    [[nodiscard]] QString registryCheckedText() const;

    // Re-reads the registry cache and scans the machine.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void backupGame(const QString &gameId);
    // Backs up a game whose saves exceed what its profile expects, after the
    // user confirmed (see largeBackupFound).
    Q_INVOKABLE void backupGameAnyway(const QString &gameId);
    // Backs up every game whose saves exist on this machine.
    Q_INVOKABLE void backupAll();

    // {name, store, reason, installDir, launcherCloud, snapshotCount, coverColor, profileSource,
    //  local, cover, iconOnly, hero, logo, saveFolders: [{url, path}]}
    Q_INVOKABLE QVariantMap gameInfo(const QString &gameId) const;
    // Newest first: [{id, date, relative, kind, kindText, files, label, inconsistent}]
    Q_INVOKABLE QVariantList snapshots(const QString &gameId) const;

    // Save slots of a game, most recently updated first:
    // [{name, active, snapshotId, date, relative, kindText, snapshots}]
    Q_INVOKABLE QVariantList saveSlots(const QString &gameId) const;
    // The slot whose files are on disk, or "" for the unnamed progress.
    Q_INVOKABLE QString activeSlot(const QString &gameId) const;
    // Backs up the current files as a new save slot named `name` and makes it
    // the active one ("save as"). Returns an error message, or "" when the
    // backup started.
    Q_INVOKABLE QString saveAsSlot(const QString &gameId, const QString &name);

    // Computes a restore plan and keeps it for restore():
    // {ok, error, create, overwrite, unchanged, skip, newer, extra, cloudConflict, gameRunning, slot, activeSlot,
    //  files: [{target, name, action, reason, newer, backupSize, currentSize}]}
    Q_INVOKABLE QVariantMap planRestore(const QString &gameId, const QString &snapshotId);
    // Executes the last plan. policy: "prefer-saveforge", "prefer-launcher" or "ask".
    Q_INVOKABLE void restore(const QString &gameId, const QString &snapshotId, const QString &policy);

    // Downloads newer profiles from the registry mirrors and refreshes the
    // library when something changed.
    Q_INVOKABLE void updateRegistry();
    // Adds a game the registry does not know, from its save folders (URLs or
    // paths), then refreshes. Returns an error message for the user, or "".
    Q_INVOKABLE QString addManualGame(const QString &name, const QVariantList &folders);
    // Stores a profile.v1 file as a local profile, then refreshes. Returns an error message or "".
    Q_INVOKABLE QString importProfile(const QUrl &file);
    // Deletes a local profile (games added by hand or imported). Backups stay in the vault.
    Q_INVOKABLE void removeLocalGame(const QString &gameId);
    // Games with at least one backup, for the export list: [{id, name, snapshots, cover, coverColor}]
    Q_INVOKABLE QVariantList exportableGames() const;
    // The previous export, for incremental exports: {exists, date, games}
    Q_INVOKABLE QVariantMap lastExport() const;
    // Writes the chosen games' backups to a .gamesave. With `incremental`,
    // files the previous export carried are left out; `includeUserFolders`
    // adds the user's folder paths to the manifest (diagnostics only).
    Q_INVOKABLE void exportGames(const QStringList &gameIds, const QUrl &file, bool allHistory, bool incremental,
                                 bool includeUserFolders);
    // Opens and checks a .gamesave without importing it:
    // {ok, error, games: [{name, snapshots}], incremental, date, sourceOs, missingBase}
    Q_INVOKABLE QVariantMap inspectGamesave(const QUrl &file) const;
    // Applies the retention policy to every game and frees the space of
    // backups no longer needed.
    Q_INVOKABLE void cleanUpVault();
    // Checks every file in the vault against its hash (vault spec section 6).
    Q_INVOKABLE void verifyVault();
    // Pinned backups are never removed by the retention policy.
    Q_INVOKABLE void setSnapshotPinned(const QString &gameId, const QString &snapshotId, bool pinned);
    // Deletes one backup (not pinned ones). Its files' space is freed by the next clean-up.
    Q_INVOKABLE void deleteSnapshot(const QString &gameId, const QString &snapshotId);

    // Adds the backups of a .gamesave to the vault; restores nothing.
    Q_INVOKABLE void importGamesave(const QUrl &file);

    // Verifies and caches a registry distribution chosen by the user, then refreshes.
    Q_INVOKABLE void importRegistry(const QUrl &folder);
    // Switches to another vault directory; returns an error message or "".
    Q_INVOKABLE QString setVaultFolder(const QUrl &folder);

signals:
    void busyChanged();
    void catalogChanged();
    void vaultChanged();
    void cloudPolicyChanged();
    void activityChanged();
    void registryChanged();
    void coversChanged();
    void autoBackupChanged();
    void retentionChanged();
    void trayAvailableChanged();
    // A manual backup stopped because the saves are larger than the profile
    // expects; the interface asks before calling backupGameAnyway().
    void largeBackupFound(const QString &gameId, const QString &name, const QString &size, const QString &expected);
    // An automatic backup finished; for notifications while the window is hidden.
    void automaticBackupDone(const QString &text);
    // The active save slot of a game changed.
    void slotsChanged(const QString &gameId);
    // A message the interface may show transiently. kind: "success", "warning", "error", "info".
    void notify(const QString &text, const QString &kind);

private:
    template <typename T>
    void run(const QString &busyText, std::function<T()> work, std::function<void(T)> done);
    void log(const QString &text, const QString &kind);
    void openVault();
    // Starts a quiet registry update when automatic updates are on and the
    // last check is more than a day old.
    void maybeUpdateRegistry();
    void runRegistryUpdate(bool quiet);
    // Downloads, in the background, the Steam covers the library still lacks
    // (when the user allows it), then updates the library's art.
    void downloadMissingCovers();
    // Runs the weekly verification and clean-up when it is due and the app is idle.
    void maybeRunMaintenance();
    [[nodiscard]] services::RetentionPolicy retentionPolicy() const;
    // Backs up the queued games whose saves changed (kind "auto").
    void runAutomaticBackups();
    // Starts a backup of one game into `slot`; `done` runs on success.
    void runBackup(const QString &gameId, const QString &slot, std::function<void()> done = {}, bool allowLarge = false);
    [[nodiscard]] services::ServiceEnvironment serviceEnvironment();
    [[nodiscard]] QString appVersion() const;

    AppSettings m_settings;
    LibraryModel m_library;
    std::optional<vault::Vault> m_vault;
    QString m_vaultError;
    QString m_busyText;
    int m_profileCount = 0;
    bool m_usingTestKeys = false;
    QStringList m_warnings;
    QStringList m_notices;
    QVariantList m_activity;
    std::optional<services::RestorePlan> m_plan;
    QString m_planGameId;
    QFutureWatcherBase *m_running = nullptr;
    bool m_autoUpdateChecked = false;
    QFutureWatcherBase *m_coverDownload = nullptr;
    bool m_trayAvailable = false;
    QTimer m_autoTimer;
    services::GameExitTracker m_exitTracker;
    // Games waiting for an automatic backup, with the trigger that queued them.
    QList<QPair<QString, vault::SnapshotTrigger>> m_autoQueue;
};

} // namespace saveforge::backend
