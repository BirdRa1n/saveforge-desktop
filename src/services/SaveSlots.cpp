#include "saveforge/services/SaveSlots.h"

#include <QHash>

#include <algorithm>

namespace saveforge::services {

QList<SaveSlot> saveSlots(const vault::Vault &vault, const QString &gameId)
{
    QHash<QString, SaveSlot> byName;
    // Oldest first, so the last snapshot seen for a slot is its head.
    for (const vault::Snapshot &snapshot : vault.snapshots(gameId)) {
        if (snapshot.slot.isEmpty())
            continue;
        SaveSlot &slot = byName[snapshot.slot];
        slot.name = snapshot.slot;
        slot.head = snapshot;
        ++slot.snapshots;
    }
    QList<SaveSlot> result = byName.values();
    std::sort(result.begin(), result.end(), [](const SaveSlot &a, const SaveSlot &b) {
        return a.head.createdAt != b.head.createdAt ? a.head.createdAt > b.head.createdAt : a.name < b.name;
    });
    return result;
}

} // namespace saveforge::services
