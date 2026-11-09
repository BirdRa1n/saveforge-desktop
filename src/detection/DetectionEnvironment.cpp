#include "saveforge/detection/DetectionEnvironment.h"

#include "saveforge/platform/UserFolders.h"

#include <QDir>
#include <QSettings>

namespace saveforge::detection {

namespace {

#if !defined(Q_OS_WIN)
class EmptyRegistry final : public RegistryReader {
public:
    QString value(const QString &, const QString &) const override { return {}; }
    QStringList subkeys(const QString &) const override { return {}; }
};
#else
class WindowsRegistry final : public RegistryReader {
public:
    QString value(const QString &key, const QString &name) const override
    {
        const QSettings settings(key, QSettings::NativeFormat);
        return settings.value(name).toString();
    }

    QStringList subkeys(const QString &key) const override
    {
        const QSettings settings(key, QSettings::NativeFormat);
        return settings.childGroups();
    }
};
#endif

} // namespace

std::shared_ptr<const RegistryReader> hostRegistry()
{
#if defined(Q_OS_WIN)
    static const auto registry = std::make_shared<const WindowsRegistry>();
#else
    static const auto registry = std::make_shared<const EmptyRegistry>();
#endif
    return registry;
}

void MapRegistryReader::set(const QString &key, const QString &name, const QString &value)
{
    m_values[key.toLower()].insert(name, value);
}

QString MapRegistryReader::value(const QString &key, const QString &name) const
{
    return m_values.value(key.toLower()).value(name);
}

QStringList MapRegistryReader::subkeys(const QString &key) const
{
    const QString prefix = key.toLower() + u'\\';
    QStringList names;
    for (auto it = m_values.cbegin(); it != m_values.cend(); ++it) {
        if (!it.key().startsWith(prefix))
            continue;
        const QString child = it.key().mid(prefix.size()).section(u'\\', 0, 0);
        if (!child.isEmpty() && !names.contains(child, Qt::CaseInsensitive))
            names.append(child);
    }
    return names;
}

DetectionEnvironment DetectionEnvironment::host()
{
    DetectionEnvironment env;
    env.hostOs = platform::hostOs();
    env.userFolders = platform::hostUserFolders();
    env.home = env.userFolders.value(QStringLiteral("home"));
    if (env.home.isEmpty())
        env.home = QDir::homePath();
    env.osUserName = qEnvironmentVariable(env.hostOs == platform::Os::Windows ? "USERNAME" : "USER");
    env.registry = hostRegistry();
    return env;
}

} // namespace saveforge::detection
