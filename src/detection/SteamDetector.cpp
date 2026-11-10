#include "savevault/detection/Launchers.h"
#include "savevault/detection/Vdf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

namespace savevault::detection {

namespace {

// Offset between a 32-bit Steam account id and its 64-bit SteamID.
constexpr quint64 kSteamId64Base = 76561197960265728ULL;

// Steam's own redistributables and compatibility tools are not games.
bool isSteamTool(const QString &appId, const QString &name)
{
    static const QSet<QString> toolIds{QStringLiteral("228980"), QStringLiteral("1070560"),
                                       QStringLiteral("1391110"), QStringLiteral("1628350")};
    return toolIds.contains(appId) || name.startsWith(QLatin1String("Proton "))
           || name == u"Proton Experimental" || name.startsWith(QLatin1String("Steam Linux Runtime"))
           || name.startsWith(QLatin1String("Steamworks Common"));
}

QString clean(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

std::optional<VdfNode> readVdf(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    auto parsed = parseVdf(file.readAll());
    if (!parsed.ok())
        return std::nullopt;
    return std::move(parsed).value();
}

// Library directories listed in libraryfolders.vdf, in either the current
// format ("0" { "path" "..." }) or the legacy one ("1" "D:\\SteamLibrary").
QStringList libraryFolders(const QString &root)
{
    QStringList libraries{root};
    for (const QString &candidate : {root + QStringLiteral("/steamapps/libraryfolders.vdf"),
                                     root + QStringLiteral("/config/libraryfolders.vdf")}) {
        const auto vdf = readVdf(candidate);
        if (!vdf)
            continue;
        const VdfNode *folders = vdf->child(u"libraryfolders");
        if (!folders)
            continue;
        for (const VdfNode &entry : folders->children) {
            bool numeric = false;
            entry.key.toInt(&numeric);
            if (!numeric)
                continue;
            const QString path = entry.isObject ? entry.valueOf(u"path") : entry.value;
            if (!path.isEmpty() && !libraries.contains(clean(path), Qt::CaseInsensitive))
                libraries.append(clean(path));
        }
    }
    return libraries;
}

QStringList steamUserIds(const QString &root)
{
    QStringList ids;
    static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
    for (const QString &name : QDir(root + QStringLiteral("/userdata")).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!digits.match(name).hasMatch() || name == u"0")
            continue;
        const quint64 accountId = name.toULongLong();
        ids.append(name);
        ids.append(QString::number(kSteamId64Base + accountId));
    }
    return ids;
}

} // namespace

QStringList steamRoots(const DetectionEnvironment &env)
{
    QStringList candidates;
    switch (env.hostOs) {
    case platform::Os::Windows:
        // InstallPath keeps the real spelling; SteamPath is often lower-cased.
        candidates << env.registry->value(QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam"),
                                          QStringLiteral("InstallPath"))
                   << env.registry->value(QStringLiteral("HKEY_CURRENT_USER\\Software\\Valve\\Steam"), QStringLiteral("SteamPath"));
        break;
    case platform::Os::Linux:
        candidates << env.home + QStringLiteral("/.steam/steam") << env.home + QStringLiteral("/.local/share/Steam")
                   << env.home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam")
                   << env.home + QStringLiteral("/snap/steam/common/.local/share/Steam");
        break;
    case platform::Os::MacOS:
        candidates << env.home + QStringLiteral("/Library/Application Support/Steam");
        break;
    }

    QStringList roots;
    QSet<QString> seen;
    for (const QString &candidate : std::as_const(candidates)) {
        if (candidate.isEmpty() || !QFileInfo(candidate + QStringLiteral("/steamapps")).isDir())
            continue;
        // ~/.steam/steam is usually a symlink to ~/.local/share/Steam, and on
        // Windows/macOS the same folder may be spelled with different case.
        QString key = QFileInfo(candidate).canonicalFilePath();
        if (env.hostOs != platform::Os::Linux)
            key = key.toLower();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        roots.append(clean(candidate));
    }
    return roots;
}

QList<InstallCandidate> detectSteam(const DetectionEnvironment &env)
{
    QList<InstallCandidate> games;
    QSet<QString> seenApps;
    for (const QString &root : steamRoots(env)) {
        const QStringList userIds = steamUserIds(root);
        for (const QString &library : libraryFolders(root)) {
            const QString steamapps = library + QStringLiteral("/steamapps");
            const QStringList manifests = QDir(steamapps).entryList({QStringLiteral("appmanifest_*.acf")}, QDir::Files);
            for (const QString &manifestName : manifests) {
                const auto vdf = readVdf(steamapps + u'/' + manifestName);
                const VdfNode *app = vdf ? vdf->child(u"AppState") : nullptr;
                if (!app)
                    continue;
                const QString appId = app->valueOf(u"appid");
                const QString name = app->valueOf(u"name");
                const QString installDir = steamapps + QStringLiteral("/common/") + app->valueOf(u"installdir");
                if (appId.isEmpty() || app->valueOf(u"installdir").isEmpty() || isSteamTool(appId, name)
                    || seenApps.contains(appId) || !QFileInfo(installDir).isDir())
                    continue;
                seenApps.insert(appId);

                InstallCandidate game;
                game.store = QStringLiteral("steam");
                game.storeId = appId;
                game.name = name;
                game.installDir = installDir;
                game.storeUserIds = userIds;
                game.gameOs = env.hostOs;

                // Proton: a prefix plus Windows executables means a Windows build.
                if (env.hostOs == platform::Os::Linux) {
                    const QString prefix = normalizeWinePrefix(steamapps + QStringLiteral("/compatdata/") + appId);
                    if (!prefix.isEmpty() && !findExecutables(installDir, platform::Os::Windows).isEmpty()) {
                        game.gameOs = platform::Os::Windows;
                        game.winePrefix = prefix;
                        game.wineUser = winePrefixUser(prefix);
                    }
                }
                game.executables = findExecutables(installDir, game.gameOs);
                games.append(game);
            }
        }
    }
    return games;
}

} // namespace savevault::detection
