#pragma once

#include <QFile>
#include <QString>

// Durability helpers for crash-safe writes (write to a temporary file, sync,
// then rename into place).

namespace savevault::platform {

// Flushes Qt's buffer and asks the OS to persist the file to disk
// (FlushFileBuffers on Windows, fsync elsewhere).
[[nodiscard]] bool syncFile(QFile &file);

// Persists a directory entry change (rename/create) on POSIX systems.
// No-op on Windows, where NTFS journals metadata changes.
void syncDirectory(const QString &directory);

} // namespace savevault::platform
