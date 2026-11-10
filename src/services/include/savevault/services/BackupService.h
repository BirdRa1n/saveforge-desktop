#pragma once

#include "savevault/core/Result.h"
#include "savevault/services/GameTarget.h"
#include "savevault/vault/Vault.h"

namespace savevault::services {

struct BackupOptions {
    vault::SnapshotKind kind = vault::SnapshotKind::Manual;
    std::optional<vault::SnapshotTrigger> trigger;
    QString label;
    QString slot;                         // save slot the files on disk belong to; empty: unnamed progress
    // Back up even when the files exceed the profile's backup.maxBytes.
    bool allowLargerThanExpected = false;
    vault::FileReadCallback onFileRead;   // progress and cancellation
};

// Backs up a game's saves into a vault.
//
// The profile is resolved on this machine and every matched file is captured
// in one snapshot. Errors:
//   - PermissionDenied: the game is running and its profile requires it closed;
//   - NotFound: the profile matched no file and no registry key;
//   - TooLarge: the files exceed the profile's maxBytes (context: the size in
//     bytes) and the options do not allow it; callers ask the user first;
//   - anything the vault reports (lock held, I/O, cancellation).
// Registry rules are captured with their subkeys when registryRulesApply().
class BackupService {
public:
    BackupService(vault::Vault &vault, ServiceEnvironment env);

    [[nodiscard]] Result<vault::Snapshot> backup(const GameTarget &target, const BackupOptions &options = {});

private:
    vault::Vault &m_vault;
    ServiceEnvironment m_env;
};

} // namespace savevault::services
