#include "saveforge/platform/Os.h"

namespace saveforge::platform {

QString osName(Os os)
{
    switch (os) {
    case Os::Windows: return QStringLiteral("windows");
    case Os::Linux: return QStringLiteral("linux");
    case Os::MacOS: return QStringLiteral("macos");
    }
    return {};
}

std::optional<Os> osFromName(QStringView name)
{
    if (name == u"windows")
        return Os::Windows;
    if (name == u"linux")
        return Os::Linux;
    if (name == u"macos")
        return Os::MacOS;
    return std::nullopt;
}

} // namespace saveforge::platform
