#pragma once

#include <QList>
#include <QString>

#include <functional>

namespace savevault::platform {

// A running process, as far as the current user can see it.
struct ProcessInfo {
    qint64 pid = 0;
    QString name;            // executable file name, e.g. "hollow_knight.exe"
    QString executablePath;  // absolute, '/'-separated; empty when not readable
};

// Processes running on this machine: Toolhelp on Windows, /proc on Linux,
// libproc on macOS. Processes whose details cannot be read (other users,
// protected system processes) are reported with whatever is available.
[[nodiscard]] QList<ProcessInfo> runningProcesses();

// Injectable source of running processes, so services can be tested.
using ProcessProvider = std::function<QList<ProcessInfo>()>;

} // namespace savevault::platform
