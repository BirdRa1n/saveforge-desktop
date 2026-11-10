#pragma once

#include <QString>

namespace savevault::platform {

// Directory where SaveVault keeps its local data (registry cache, settings,
// index), shared by the GUI and savevault-cli:
//   Windows: %LOCALAPPDATA%/SaveVault
//   Linux:   $XDG_DATA_HOME/SaveVault (~/.local/share/SaveVault)
//   macOS:   ~/Library/Application Support/SaveVault
// The SAVEVAULT_DATA_DIR environment variable overrides it (tests, portable use).
// A directory left by versions named SaveForge is renamed to SaveVault on the
// first call.
[[nodiscard]] QString appDataDirectory();

} // namespace savevault::platform
