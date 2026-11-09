#include "Commands.h"

#include "saveforge/detection/GameMatcher.h"
#include "saveforge/detection/Launchers.h"
#include "saveforge/profiles/ProfileParser.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>

namespace saveforge::cli {

namespace {

QString osLabel(platform::Os os)
{
    return platform::osName(os);
}

QString reasonLabel(detection::MatchReason reason)
{
    switch (reason) {
    case detection::MatchReason::StoreId: return QStringLiteral("store id");
    case detection::MatchReason::Executable: return QStringLiteral("executable");
    case detection::MatchReason::InstallDirName: return QStringLiteral("folder name");
    case detection::MatchReason::SavesFound: return QStringLiteral("saves found");
    }
    return {};
}

QList<profiles::Profile> loadProfiles(const QString &dir, QTextStream &err)
{
    QList<profiles::Profile> loaded;
    for (const QFileInfo &info : QDir(dir).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        auto profile = profiles::parseProfile(file.readAll());
        if (profile.ok())
            loaded.append(std::move(profile).value());
        else
            err << "skipping " << info.fileName() << ": " << profile.error().toString() << Qt::endl;
    }
    return loaded;
}

} // namespace

int runDetect(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("List installed games and, optionally, match them to profiles."));
    parser.addHelpOption();
    const QCommandLineOption profilesOption(QStringLiteral("profiles"),
                                            QStringLiteral("Directory with profile JSON files to match against."),
                                            QStringLiteral("dir"));
    const QCommandLineOption noScanOption(QStringLiteral("no-save-scan"),
                                          QStringLiteral("Do not scan for saves of games that are not installed."));
    parser.addOption(profilesOption);
    parser.addOption(noScanOption);
    if (!parser.parse(arguments)) {
        err << parser.errorText() << Qt::endl;
        return 2;
    }
    if (parser.isSet(QStringLiteral("help"))) {
        out << parser.helpText();
        return 0;
    }

    const detection::DetectionEnvironment env = detection::DetectionEnvironment::host();
    const QStringList roots = detection::steamRoots(env);
    for (const QString &root : roots)
        out << "Steam root: " << root << Qt::endl;

    const QList<detection::InstallCandidate> installs = detection::detectInstalledGames(env);
    out << installs.size() << " installed game(s):" << Qt::endl;
    for (const detection::InstallCandidate &game : installs) {
        out << "  [" << game.store;
        if (!game.launcher.isEmpty())
            out << " via " << game.launcher;
        out << "] " << game.storeId << "  " << game.name << "  (" << osLabel(game.gameOs) << ")" << Qt::endl
            << "      " << game.installDir << Qt::endl;
        if (!game.winePrefix.isEmpty())
            out << "      prefix: " << game.winePrefix << " (user " << game.wineUser << ")" << Qt::endl;
    }

    if (!parser.isSet(profilesOption))
        return 0;

    const QList<profiles::Profile> profiles = loadProfiles(parser.value(profilesOption), err);
    out << Qt::endl << profiles.size() << " profile(s) loaded." << Qt::endl;
    const detection::DetectionReport report =
        detection::detectGames(profiles, installs, env, !parser.isSet(noScanOption));
    out << report.games.size() << " game(s) detected:" << Qt::endl;
    for (const detection::DetectedGame &game : report.games) {
        out << "  " << game.profileId << "  <- " << reasonLabel(game.reason);
        if (game.reason == detection::MatchReason::SavesFound)
            out << " (" << game.saveFilesFound << " file(s))";
        out << Qt::endl;
    }
    out << report.unmatched.size() << " installed game(s) without a profile." << Qt::endl;
    return 0;
}

} // namespace saveforge::cli
