#pragma once

#include "saveforge/core/Result.h"
#include "saveforge/services/GameTarget.h"
#include "saveforge/vault/Vault.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <optional>

namespace saveforge::services {

// Who wins when the game's launcher also syncs saves to its own cloud
// (saveforge-docs planning section 5.6.1). `Ask` must be resolved by the user
// interface into one of the other two before a restore is executed.
enum class CloudPolicy { Ask, PreferLauncher, PreferSaveForge };

enum class FileAction {
    Create,     // the file does not exist here and will be written
    Overwrite,  // the file exists with different content and will be replaced
    Unchanged,  // the file already has the snapshot's content
    Skip,       // cannot be restored on this machine (see skipReason)
};

struct PlannedFile {
    vault::SnapshotFile source;
    QString target;              // absolute path; empty when skipped
    FileAction action = FileAction::Skip;
    QString skipReason;
    bool currentIsNewer = false; // existing file was modified after the snapshot was taken
};

// What a restore would do, computed without touching any file.
struct RestorePlan {
    vault::Snapshot snapshot;
    QList<PlannedFile> files;
    QStringList extraFiles;            // matched by the profile today but absent from the snapshot; left untouched
    bool launcherCloudConflict = false;
    bool gameRunning = false;
    // Registry keys of the snapshot whose values differ from the registry now
    // (written by execute()); keys the snapshot has but this machine cannot
    // write, because registry rules do not apply here, are counted in skippedRegistryKeys.
    QList<vault::RegistryKey> registryKeys;
    qsizetype skippedRegistryKeys = 0;

    [[nodiscard]] qsizetype count(FileAction action) const;
    [[nodiscard]] bool hasChanges() const
    {
        return count(FileAction::Create) + count(FileAction::Overwrite) > 0 || !registryKeys.isEmpty();
    }
    [[nodiscard]] bool hasNewerLocalFiles() const;
};

struct RestoreReport {
    qsizetype written = 0;
    qsizetype unchanged = 0;
    qsizetype skipped = 0;
    std::optional<QUuid> preRestoreSnapshotId;   // undo point, when there was something to protect
};

// Restores snapshots (saveforge-specs/vault/SPEC.md section 4).
class RestoreService {
public:
    RestoreService(vault::Vault &vault, ServiceEnvironment env);

    // Computes the plan for restoring `snapshotId` of the target's game.
    // `bindingOverrides` replaces snapshot binding values, e.g. to restore the
    // saves of one Steam account into another ({"storeUserId": "<new id>"}).
    [[nodiscard]] Result<RestorePlan> plan(const GameTarget &target, const QUuid &snapshotId,
                                           const QHash<QString, QString> &bindingOverrides = {}) const;

    // Executes a plan. Refused while the game runs and, on a launcher-cloud
    // conflict, unless the policy is PreferSaveForge. Before writing, the
    // current files are captured in a "pre-restore" snapshot that belongs to
    // `currentSlot`, the save slot those files are part of (so switching slots
    // keeps the progress of the slot being left). All files are first written
    // next to their targets and verified, then swapped in with renames; any
    // failure restores the previous files.
    [[nodiscard]] Result<RestoreReport> execute(const GameTarget &target, const RestorePlan &plan, CloudPolicy policy,
                                                const QString &currentSlot = {});

private:
    vault::Vault &m_vault;
    ServiceEnvironment m_env;
};

} // namespace saveforge::services
