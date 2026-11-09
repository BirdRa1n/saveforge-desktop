#include "saveforge/detection/Launchers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace saveforge::detection {

namespace {

QString clean(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QJsonDocument readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll());
}

QStringList heroicConfigDirs(const DetectionEnvironment &env)
{
    QStringList dirs;
    switch (env.hostOs) {
    case platform::Os::Linux:
        dirs << env.home + QStringLiteral("/.config/heroic")
             << env.home + QStringLiteral("/.var/app/com.heroicgameslauncher.hgl/config/heroic");
        break;
    case platform::Os::Windows:
        if (const QString appData = env.userFolders.value(QStringLiteral("winAppData")); !appData.isEmpty())
            dirs << appData + QStringLiteral("/heroic");
        break;
    case platform::Os::MacOS:
        dirs << env.home + QStringLiteral("/Library/Application Support/heroic");
        break;
    }
    dirs.removeIf([](const QString &dir) { return !QFileInfo(dir).isDir(); });
    return dirs;
}

std::optional<platform::Os> heroicPlatform(const QString &value)
{
    const QString p = value.toLower();
    if (p == u"windows")
        return platform::Os::Windows;
    if (p == u"linux")
        return platform::Os::Linux;
    if (p == u"mac" || p == u"osx" || p == u"macos")
        return platform::Os::MacOS;
    return std::nullopt;
}

// Fills prefix information for a Windows build on a non-Windows host from
// GamesConfig/<appName>.json ({"<appName>": {"winePrefix": "..."}}).
void applyWinePrefix(InstallCandidate &game, const QString &configDir, const DetectionEnvironment &env)
{
    if (game.gameOs != platform::Os::Windows || env.hostOs == platform::Os::Windows)
        return;
    const QJsonObject config = readJson(configDir + QStringLiteral("/GamesConfig/") + game.storeId + QStringLiteral(".json"))
                                   .object()
                                   .value(game.storeId)
                                   .toObject();
    const QString prefix = normalizeWinePrefix(config.value(QLatin1String("winePrefix")).toString());
    if (!prefix.isEmpty()) {
        game.winePrefix = prefix;
        game.wineUser = winePrefixUser(prefix);
    }
}

InstallCandidate makeCandidate(const QString &store, const QString &id, const QString &title, const QString &installPath,
                               platform::Os os)
{
    InstallCandidate game;
    game.store = store;
    game.launcher = QStringLiteral("heroic");
    game.storeId = id;
    game.installDir = clean(installPath);
    game.name = title.isEmpty() ? QFileInfo(game.installDir).fileName() : title;
    game.gameOs = os;
    game.executables = findExecutables(game.installDir, os);
    return game;
}

} // namespace

QList<InstallCandidate> detectHeroic(const DetectionEnvironment &env)
{
    QList<InstallCandidate> games;
    for (const QString &configDir : heroicConfigDirs(env)) {
        // Epic games installed through Legendary.
        const QJsonObject legendary =
            readJson(configDir + QStringLiteral("/legendaryConfig/legendary/installed.json")).object();
        for (auto it = legendary.begin(); it != legendary.end(); ++it) {
            const QJsonObject entry = it.value().toObject();
            const QString installPath = entry.value(QLatin1String("install_path")).toString();
            const auto os = heroicPlatform(entry.value(QLatin1String("platform")).toString(QStringLiteral("windows")));
            if (installPath.isEmpty() || !os || !QFileInfo(installPath).isDir()
                || entry.value(QLatin1String("is_dlc")).toBool())
                continue;
            InstallCandidate game = makeCandidate(QStringLiteral("epic"), it.key(),
                                                  entry.value(QLatin1String("title")).toString(), installPath, *os);
            applyWinePrefix(game, configDir, env);
            games.append(game);
        }

        // GOG games.
        const QJsonArray gog = readJson(configDir + QStringLiteral("/gog_store/installed.json"))
                                   .object()
                                   .value(QLatin1String("installed"))
                                   .toArray();
        for (const QJsonValue &value : gog) {
            const QJsonObject entry = value.toObject();
            const QString appName = entry.value(QLatin1String("appName")).toString();
            const QString installPath = entry.value(QLatin1String("install_path")).toString();
            const auto os = heroicPlatform(entry.value(QLatin1String("platform")).toString(QStringLiteral("windows")));
            if (appName.isEmpty() || installPath.isEmpty() || !os || !QFileInfo(installPath).isDir()
                || entry.value(QLatin1String("is_dlc")).toBool())
                continue;
            InstallCandidate game = makeCandidate(QStringLiteral("gog"), appName,
                                                  entry.value(QLatin1String("title")).toString(), installPath, *os);
            applyWinePrefix(game, configDir, env);
            games.append(game);
        }
    }
    return games;
}

QList<InstallCandidate> detectInstalledGames(const DetectionEnvironment &env)
{
    return detectSteam(env) + detectEpic(env) + detectGog(env) + detectHeroic(env);
}

} // namespace saveforge::detection
