#pragma once

#include <QString>

namespace saveforge::platform {

// Directory where SaveForge keeps its local data (registry cache, settings,
// index), shared by the GUI and saveforge-cli:
//   Windows: %LOCALAPPDATA%/SaveForge
//   Linux:   $XDG_DATA_HOME/SaveForge (~/.local/share/SaveForge)
//   macOS:   ~/Library/Application Support/SaveForge
// The SAVEFORGE_DATA_DIR environment variable overrides it (tests, portable use).
[[nodiscard]] QString appDataDirectory();

} // namespace saveforge::platform
