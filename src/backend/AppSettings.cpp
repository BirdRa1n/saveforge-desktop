#include "AppSettings.h"

#include "saveforge/core/Uuid.h"
#include "saveforge/platform/AppPaths.h"
#include "saveforge/registry/RegistryUpdater.h"

#include <QDir>
#include <QRegularExpression>
#include <QSettings>

#include <algorithm>

namespace saveforge::backend {

namespace {
// Cyan, the default accent of the Console x Studio design.
constexpr const char *kDefaultAccent = "#22D3EE";
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

int AppSettings::retentionKeepLast() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return std::max(0, settings.value(QStringLiteral("retention/keepLast"), 0).toInt());
}

void AppSettings::setRetentionKeepLast(int count)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("retention/keepLast"), std::max(0, count));
}

int AppSettings::retentionKeepDays() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return std::max(0, settings.value(QStringLiteral("retention/keepDays"), 0).toInt());
}

void AppSettings::setRetentionKeepDays(int days)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("retention/keepDays"), std::max(0, days));
}

QDateTime AppSettings::lastMaintenance() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    return QDateTime::fromString(settings.value(QStringLiteral("maintenance/last")).toString(), Qt::ISODate);
}

void AppSettings::setLastMaintenance(const QDateTime &when)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("maintenance/last"), when.toUTC().toString(Qt::ISODate));
}

QString AppSettings::exportLogDirectory() const
{
    return m_dataDir + QStringLiteral("/exports");
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

QString AppSettings::themeMode() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    const QString mode = settings.value(QStringLiteral("appearance/theme")).toString();
    return mode == QLatin1String("dark") || mode == QLatin1String("light") ? mode : QStringLiteral("system");
}

void AppSettings::setThemeMode(const QString &mode)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/theme"), mode);
}

QString AppSettings::accentColor() const
{
    const QSettings settings(file(), QSettings::IniFormat);
    const QString color = settings.value(QStringLiteral("appearance/accent")).toString();
    static const QRegularExpression hex(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return hex.match(color).hasMatch() ? color.toUpper() : QString::fromLatin1(kDefaultAccent);
}

void AppSettings::setAccentColor(const QString &color)
{
    QSettings settings(file(), QSettings::IniFormat);
    settings.setValue(QStringLiteral("appearance/accent"), color);
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
