#pragma once

#include <QString>
#include <QStringView>

#include <optional>

namespace saveforge::platform {

// Operating system family, as used by profiles (`when.os`) and placeholders.
enum class Os {
    Windows,
    Linux,
    MacOS,
};

// The OS this binary was built for.
[[nodiscard]] constexpr Os hostOs() noexcept
{
#if defined(Q_OS_WIN)
    return Os::Windows;
#elif defined(Q_OS_MACOS)
    return Os::MacOS;
#else
    return Os::Linux;
#endif
}

// Spec names: "windows", "linux", "macos".
[[nodiscard]] QString osName(Os os);
[[nodiscard]] std::optional<Os> osFromName(QStringView name);

} // namespace saveforge::platform
