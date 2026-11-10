#include "savevault/platform/Processes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#if defined(Q_OS_WIN)
#  include <qt_windows.h>
#  include <tlhelp32.h>
#elif defined(Q_OS_MACOS)
#  include <libproc.h>
#  include <sys/proc_info.h>
#  include <vector>
#endif

namespace savevault::platform {

#if defined(Q_OS_WIN)

namespace {

QString imagePath(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    wchar_t buffer[MAX_PATH * 4];
    DWORD size = static_cast<DWORD>(std::size(buffer));
    QString path;
    if (QueryFullProcessImageNameW(process, 0, buffer, &size))
        path = QDir::fromNativeSeparators(QString::fromWCharArray(buffer, static_cast<qsizetype>(size)));
    CloseHandle(process);
    return path;
}

} // namespace

QList<ProcessInfo> runningProcesses()
{
    QList<ProcessInfo> processes;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return processes;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    for (BOOL ok = Process32FirstW(snapshot, &entry); ok; ok = Process32NextW(snapshot, &entry)) {
        processes.append({static_cast<qint64>(entry.th32ProcessID), QString::fromWCharArray(entry.szExeFile),
                          imagePath(entry.th32ProcessID)});
    }
    CloseHandle(snapshot);
    return processes;
}

#elif defined(Q_OS_MACOS)

QList<ProcessInfo> runningProcesses()
{
    QList<ProcessInfo> processes;
    const int count = proc_listallpids(nullptr, 0);
    if (count <= 0)
        return processes;
    std::vector<pid_t> pids(static_cast<size_t>(count) * 2);
    const int found = proc_listallpids(pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
    for (int i = 0; i < found; ++i) {
        char path[PROC_PIDPATHINFO_MAXSIZE] = {};
        if (proc_pidpath(pids[static_cast<size_t>(i)], path, sizeof(path)) <= 0)
            continue;
        const QString executable = QString::fromUtf8(path);
        processes.append({pids[static_cast<size_t>(i)], QFileInfo(executable).fileName(), executable});
    }
    return processes;
}

#else

QList<ProcessInfo> runningProcesses()
{
    // Linux: /proc/<pid>/exe (may be unreadable for other users' processes)
    // and /proc/<pid>/cmdline, whose first argument also covers Wine/Proton
    // games, where exe points to the Wine loader.
    QList<ProcessInfo> processes;
    const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool numeric = false;
        const qint64 pid = entry.toLongLong(&numeric);
        if (!numeric)
            continue;
        const QString base = QStringLiteral("/proc/") + entry;
        const QString exe = QFileInfo(base + QStringLiteral("/exe")).symLinkTarget();

        QString commandName;
        QFile cmdline(base + QStringLiteral("/cmdline"));
        if (cmdline.open(QIODevice::ReadOnly)) {
            const QByteArray first = cmdline.readAll().split('\0').value(0);
            // Wine passes Windows paths ("C:\\Games\\Game.exe"): keep the file name only.
            commandName = QString::fromLocal8Bit(first).replace(u'\\', u'/').section(u'/', -1);
        }
        processes.append({pid, commandName.isEmpty() ? QFileInfo(exe).fileName() : commandName, exe});
    }
    return processes;
}

#endif

} // namespace savevault::platform
