#pragma once

#include "AppSettings.h"
#include "LibraryModel.h"

#include "saveforge/services/Library.h"
#include "saveforge/services/RestoreService.h"
#include "saveforge/vault/Vault.h"

#include <QFutureWatcher>
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
    void setAutoUpdateRegistry(bool enabled);
    // "Verificado há 2 horas", "Ainda não verificado", for the settings page.
    [[nodiscard]] QString registryCheckedText() const;

    // Re-reads the registry cache and scans the machine.
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void backupGame(const QString &gameId);
    // Backs up every game whose saves exist on this machine.
    Q_INVOKABLE void backupAll();

    // {name, store, reason, installDir, launcherCloud, cloudStores, snapshotCount, saveFiles}
    Q_INVOKABLE QVariantMap gameInfo(const QString &gameId) const;
    // Newest first: [{id, date, relative, kind, kindText, files, label, inconsistent}]
    Q_INVOKABLE QVariantList snapshots(const QString &gameId) const;

    // Computes a restore plan and keeps it for restore():
    // {ok, error, create, overwrite, unchanged, skip, newer, extra, cloudConflict, gameRunning, files: [{target, action}]}
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
};

} // namespace saveforge::backend
