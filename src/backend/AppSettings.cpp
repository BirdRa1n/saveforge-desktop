#include "AppSettings.h"

#include "saveforge/core/Uuid.h"
#include "saveforge/platform/AppPaths.h"

#include <QDir>
#include <QSettings>

namespace saveforge::backend {

namespace {
const QStringList kPolicies{QStringLiteral("ask"), QStringLiteral("prefer-launcher"), QStringLiteral("prefer-saveforge")};
}

AppSettings::AppSettings()
    : m_dataDir(platform::appDataDirectory())
{
    QDir().mkpath(m_dataDir);
}

QString AppSettings::file() const
{
    return m_dataDir + QStringLiteral("/settings.ini");
}

QString AppSettings::registryCacheDirectory() const
{
    return m_dataDir + QStringLiteral("/registry");
}

QString AppSettings::vaultPath() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return settings.value(QStringLiteral("vault/path"), m_dataDir + QStringLiteral("/vault")).toString();
}

void AppSettings::setVaultPath(const QString &path)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("vault/path"), QDir::cleanPath(QDir::fromNativeSeparators(path)));
}

QUuid AppSettings::installationId()
{
    QSettings settings(file(), QSettings::IniFormat);
    QUuid id = QUuid::fromString(settings.value(QStringLiteral("installation/id")).toString());
    if (id.isNull()) {
        id = createUuidV7();
        settings.setValue(QStringLiteral("installation/id"), uuidToString(id));
    }
    return id;
}

QString AppSettings::cloudPolicy() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    const QString policy = settings.value(QStringLiteral("restore/cloudPolicy"), kPolicies.first()).toString();
    return kPolicies.contains(policy) ? policy : kPolicies.first();
}

void AppSettings::setCloudPolicy(const QString &policy)
{
    if (!kPolicies.contains(policy))
        return;
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("restore/cloudPolicy"), policy);
}

} // namespace saveforge::backend
