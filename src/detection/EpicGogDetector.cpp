#include "saveforge/detection/Launchers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace saveforge::detection {

namespace {

QString clean(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString epicManifestsDir(const DetectionEnvironment &env)
{
    switch (env.hostOs) {
    case platform::Os::Windows: {
        const QString programData = env.userFolders.value(QStringLiteral("winProgramData"));
        return programData.isEmpty() ? QString()
                                     : programData + QStringLiteral("/Epic/EpicGamesLauncher/Data/Manifests");
    }
    case platform::Os::MacOS:
        return env.home + QStringLiteral("/Library/Application Support/Epic/EpicGamesLauncher/Data/Manifests");
    case platform::Os::Linux:
        return {}; // Epic has no Linux client; see detectHeroic().
    }
    return {};
}

} // namespace

QList<InstallCandidate> detectEpic(const DetectionEnvironment &env)
{
    QList<InstallCandidate> games;
    const QString dir = epicManifestsDir(env);
    if (dir.isEmpty())
        return games;

    for (const QFileInfo &info : QDir(dir).entryInfoList({QStringLiteral("*.item")}, QDir::Files, QDir::Name)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject item = QJsonDocument::fromJson(file.readAll()).object();
        const QString appName = item.value(QLatin1String("AppName")).toString();
        const QString mainApp = item.value(QLatin1String("MainGameAppName")).toString(appName);
        const QString installDir = clean(item.value(QLatin1String("InstallLocation")).toString());
        if (appName.isEmpty() || appName != mainApp || item.value(QLatin1String("bIsIncompleteInstall")).toBool()
            || installDir.isEmpty() || !QFileInfo(installDir).isDir())
            continue;

        InstallCandidate game;
        game.store = QStringLiteral("epic");
        game.storeId = appName;
        game.name = item.value(QLatin1String("DisplayName")).toString(appName);
        game.installDir = installDir;
        game.gameOs = env.hostOs;
        game.executables = findExecutables(installDir, game.gameOs);
        const QString launch = QFileInfo(item.value(QLatin1String("LaunchExecutable")).toString()).fileName();
        if (!launch.isEmpty() && !game.executables.contains(launch, Qt::CaseInsensitive))
            game.executables.prepend(launch);
        games.append(game);
    }
    return games;
}

QList<InstallCandidate> detectGog(const DetectionEnvironment &env)
{
    QList<InstallCandidate> games;
    if (env.hostOs != platform::Os::Windows)
        return games; // GOG games on Linux and macOS are found through Heroic.

    QSet<QString> seen;
    for (const QString &root : {QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games"),
                                QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\Games")}) {
        for (const QString &subkey : env.registry->subkeys(root)) {
            const QString key = root + u'\\' + subkey;
            const QString productId = env.registry->value(key, QStringLiteral("gameID")).isEmpty()
                                          ? subkey
                                          : env.registry->value(key, QStringLiteral("gameID"));
            const QString installDir = clean(env.registry->value(key, QStringLiteral("path")));
            if (seen.contains(productId) || installDir.isEmpty() || !QFileInfo(installDir).isDir())
                continue;
            seen.insert(productId);

            InstallCandidate game;
            game.store = QStringLiteral("gog");
            game.storeId = productId;
            game.name = env.registry->value(key, QStringLiteral("gameName"));
            game.installDir = installDir;
            game.gameOs = platform::Os::Windows;
            game.executables = findExecutables(installDir, game.gameOs);
            const QString exe = env.registry->value(key, QStringLiteral("exeFile"));
            if (!exe.isEmpty() && !game.executables.contains(exe, Qt::CaseInsensitive))
                game.executables.prepend(exe);
            games.append(game);
        }
    }
    return games;
}

} // namespace saveforge::detection
