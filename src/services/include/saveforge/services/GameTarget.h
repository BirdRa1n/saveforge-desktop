#pragma once

#include "saveforge/platform/Processes.h"
#include "saveforge/profiles/PathResolver.h"
#include "saveforge/profiles/Profile.h"

#include <QList>
#include <QString>
#include <QUuid>

namespace saveforge::services {

// One game on this machine: its profile and where that profile resolves.
// Usually built from a detection result (detection::resolveContextFor).
struct GameTarget {
    profiles::Profile profile;
    QString channel = QStringLiteral("official"); // registry channel the profile came from, or "local"
    QString gameId;                               // vault game id; empty means profile.id
    profiles::ResolveContext context;

    [[nodiscard]] QString effectiveGameId() const { return gameId.isEmpty() ? profile.id : gameId; }
};

// Identity of this installation and system access that tests replace.
struct ServiceEnvironment {
    QUuid machineId;
    QString appVersion;
    platform::ProcessProvider processes = &platform::runningProcesses;
};

// Whether the game appears to be running: a process name listed in the
// profile's backup.processNames, or any process whose executable lives inside
// the game's install folder (<base>). Comparisons ignore case.
[[nodiscard]] bool isGameRunning(const GameTarget &target, const QList<platform::ProcessInfo> &processes);

} // namespace saveforge::services
