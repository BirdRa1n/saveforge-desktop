#pragma once

#include "saveforge/detection/DetectionEnvironment.h"
#include "saveforge/detection/InstallCandidate.h"

#include <QList>
#include <QStringList>

namespace saveforge::detection {

// Launcher detectors. Each one only reads launcher metadata (files or registry);
// none of them starts a launcher or needs it to be running. Missing launchers,
// unreadable files and uninstalled entries are skipped silently.

// Steam: libraryfolders.vdf, appmanifest_*.acf, userdata/<accountId>.
// On Linux, a Windows build with a compatdata/<appid>/pfx prefix is reported
// with that Proton prefix. Store user ids include both the 32-bit account id
// and the 64-bit SteamID of every account in userdata/.
[[nodiscard]] QList<InstallCandidate> detectSteam(const DetectionEnvironment &env);

// Steam installation directories that exist on this machine (for diagnostics).
[[nodiscard]] QStringList steamRoots(const DetectionEnvironment &env);

// Epic Games Launcher: *.item manifests (Windows and macOS). DLC entries and
// incomplete installs are skipped.
[[nodiscard]] QList<InstallCandidate> detectEpic(const DetectionEnvironment &env);

// GOG Galaxy and GOG offline installers on Windows: registry entries under
// SOFTWARE\GOG.com\Games (both registry views).
[[nodiscard]] QList<InstallCandidate> detectGog(const DetectionEnvironment &env);

// Heroic Games Launcher (Epic via Legendary, and GOG), including its Wine or
// Proton prefixes and the Flatpak installation.
[[nodiscard]] QList<InstallCandidate> detectHeroic(const DetectionEnvironment &env);

// All detectors above.
[[nodiscard]] QList<InstallCandidate> detectInstalledGames(const DetectionEnvironment &env);

} // namespace saveforge::detection
