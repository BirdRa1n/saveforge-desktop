#pragma once

#include "savevault/platform/Processes.h"
#include "savevault/platform/RegistryStore.h"
#include "savevault/profiles/PathResolver.h"
#include "savevault/profiles/Profile.h"

#include <QList>
#include <QString>
#include <QUuid>

namespace savevault::services {

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
    // The user's registry, for registry save rules (unavailable outside Windows).
    std::shared_ptr<platform::RegistryStore> registry = platform::hostRegistryStore();
    platform::Os hostOs = platform::hostOs();
};

// Whether the target's registry rules apply here: a Windows build running
// natively on Windows. Registries inside Wine/Proton prefixes are not handled yet.
[[nodiscard]] bool registryRulesApply(const GameTarget &target, const ServiceEnvironment &env);

// Whether the game appears to be running: a process name listed in the
// profile's backup.processNames, or any process whose executable lives inside
// the game's install folder (<base>). Comparisons ignore case.
[[nodiscard]] bool isGameRunning(const GameTarget &target, const QList<platform::ProcessInfo> &processes);

// Folders that hold the game's saves on this machine, for opening them in the
// file manager: the fixed part of each save path (up to the first glob or
// segment placeholder) of the rules that apply here, when it exists. A folder
// inside another one is left out, and folders that contain save files the
// profile matches come first. Never returns a root placeholder's folder itself
// (such as the whole Documents folder) when a deeper one exists.
[[nodiscard]] QStringList saveFolders(const GameTarget &target);

} // namespace savevault::services
