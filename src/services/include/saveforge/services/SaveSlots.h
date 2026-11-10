#pragma once

#include "saveforge/vault/Snapshot.h"
#include "saveforge/vault/Vault.h"

#include <QList>
#include <QString>

namespace saveforge::services {

// A save slot: a named line of progress of one game that the user can switch
// to and continue later (saveforge-specs/vault/SPEC.md, snapshot `slot`).
struct SaveSlot {
    QString name;
    vault::Snapshot head;     // newest snapshot of the slot: its current state
    qsizetype snapshots = 0;  // snapshots that belong to the slot
};

// The named slots of a game, most recently updated first. Snapshots without a
// slot (the unnamed progress) are not listed.
[[nodiscard]] QList<SaveSlot> saveSlots(const vault::Vault &vault, const QString &gameId);

} // namespace saveforge::services
