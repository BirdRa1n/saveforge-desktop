#pragma once

#include "saveforge/core/Result.h"
#include "saveforge/services/GameTarget.h"
#include "saveforge/vault/Vault.h"

namespace saveforge::services {

struct BackupOptions {
    vault::SnapshotKind kind = vault::SnapshotKind::Manual;
    std::optional<vault::SnapshotTrigger> trigger;
    QString label;
    vault::FileReadCallback onFileRead;   // progress and cancellation
};

// Backs up a game's saves into a vault.
//
// The profile is resolved on this machine and every matched file is captured
// in one snapshot. Errors:
//   - PermissionDenied: the game is running and its profile requires it closed;
//   - NotFound: the profile matched no file;
//   - anything the vault reports (lock held, I/O, cancellation).
// Windows registry rules are not captured yet.
class BackupService {
public:
    BackupService(vault::Vault &vault, ServiceEnvironment env);

    [[nodiscard]] Result<vault::Snapshot> backup(const GameTarget &target, const BackupOptions &options = {});

private:
    vault::Vault &m_vault;
    ServiceEnvironment m_env;
};

} // namespace saveforge::services
