#include "saveforge/platform/FileSync.h"

#if defined(Q_OS_WIN)
#  include <io.h>
#  include <qt_windows.h>
#else
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace saveforge::platform {

bool syncFile(QFile &file)
{
    if (!file.flush())
        return false;
#if defined(Q_OS_WIN)
    const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(file.handle()));
    return handle != INVALID_HANDLE_VALUE && FlushFileBuffers(handle) != 0;
#else
    return ::fsync(file.handle()) == 0;
#endif
}

void syncDirectory([[maybe_unused]] const QString &directory)
{
#if !defined(Q_OS_WIN)
    const int fd = ::open(QFile::encodeName(directory).constData(), O_RDONLY);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
#endif
}

} // namespace saveforge::platform
