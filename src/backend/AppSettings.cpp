#include "AppSettings.h"

#include "saveforge/core/Uuid.h"
#include "saveforge/platform/AppPaths.h"
#include "saveforge/registry/RegistryUpdater.h"

#include <QDir>
#include <QSettings>

#include <algorithm>

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

QString AppSettings::localProfilesDirectory() const
{
    return m_dataDir + QStringLiteral("/profiles");
}

QString AppSettings::activeSlot(const QString &gameId) const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return settings.value(QStringLiteral("activeSlots/") + gameId).toString();
}

void AppSettings::setActiveSlot(const QString &gameId, const QString &slot)
{
    QSettings settings(file(), QSettings::IniFormat);
    if (slot.isEmpty())
        settings.remove(QStringLiteral("activeSlots/") + gameId);
    else
        settings.setValue(QStringLiteral("activeSlots/") + gameId, slot);
}

bool AppSettings::backupOnGameExit() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return settings.value(QStringLiteral("autoBackup/onGameExit"), true).toBool();
}

void AppSettings::setBackupOnGameExit(bool enabled)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("autoBackup/onGameExit"), enabled);
}

int AppSettings::backupIntervalHours() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return std::clamp(settings.value(QStringLiteral("autoBackup/intervalHours"), 0).toInt(), 0, 24 * 7);
}

void AppSettings::setBackupIntervalHours(int hours)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("autoBackup/intervalHours"), std::clamp(hours, 0, 24 * 7));
}

QDateTime AppSettings::lastScheduledBackup() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return QDateTime::fromString(settings.value(QStringLiteral("autoBackup/lastScheduled")).toString(), Qt::ISODate);
}

void AppSettings::setLastScheduledBackup(const QDateTime &when)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("autoBackup/lastScheduled"), when.toUTC().toString(Qt::ISODate));
}

bool AppSettings::runInBackground() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return settings.value(QStringLiteral("app/runInBackground"), true).toBool();
}

void AppSettings::setRunInBackground(bool enabled)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("app/runInBackground"), enabled);
}

QString AppSettings::coverCacheDirectory() const
{
    return m_dataDir + QStringLiteral("/covers");
}

bool AppSettings::downloadCovers() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return settings.value(QStringLiteral("covers/download"), true).toBool();
}

void AppSettings::setDownloadCovers(bool enabled)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("covers/download"), enabled);
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

QStringList AppSettings::registryMirrors() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    const QStringList mirrors = settings.value(QStringLiteral("registry/mirrors")).toStringList();
    return mirrors.isEmpty() ? registry::builtinMirrors() : mirrors;
}

bool AppSettings::autoUpdateRegistry() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return settings.value(QStringLiteral("registry/autoUpdate"), true).toBool();
}

void AppSettings::setAutoUpdateRegistry(bool enabled)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("registry/autoUpdate"), enabled);
}

QDateTime AppSettings::lastRegistryCheck() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return QDateTime::fromString(settings.value(QStringLiteral("registry/lastCheck")).toString(), Qt::ISODate);
}

void AppSettings::setLastRegistryCheck(const QDateTime &when)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("registry/lastCheck"), when.toUTC().toString(Qt::ISODate));
}

} // namespace saveforge::backend
