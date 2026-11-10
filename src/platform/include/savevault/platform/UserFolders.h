#pragma once

#include "savevault/platform/FolderMap.h"

#include <QString>

namespace savevault::platform {

// Root placeholders of the current OS user on the host system
// (see savevault-specs/profile/placeholders.md).
//
// On Windows, known folders are resolved through SHGetKnownFolderPath so that
// relocated folders (e.g. Documents moved to another drive) are honored.
// On Linux, XDG variables are honored when they hold absolute paths.
[[nodiscard]] FolderMap hostUserFolders();

// Windows placeholders resolved inside a Wine/Proton prefix.
//   prefixDir: the directory that contains drive_c (e.g. .../compatdata/<appid>/pfx)
//   userName:  the Windows user inside the prefix ("steamuser" for Proton)
// Includes <home> mapped to the prefix user's profile, because Windows game
// builds see that directory as their home.
[[nodiscard]] FolderMap winePrefixFolders(const QString &prefixDir, const QString &userName);

} // namespace savevault::platform
