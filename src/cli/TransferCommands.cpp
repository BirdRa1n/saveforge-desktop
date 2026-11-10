// `export` and `import`: moving backups between machines as .gamesave files.

#include "CliSupport.h"
#include "Commands.h"

#include "saveforge/archive/Gamesave.h"
#include "saveforge/platform/Os.h"
#include "saveforge/services/LocalProfiles.h"
#include "saveforge/services/Transfer.h"

#include <QCommandLineParser>
#include <QJsonDocument>

namespace saveforge::cli {

namespace {

std::optional<vault::Vault> openVaultAt(const QString &dir, QTextStream &err)
{
    auto vault = vault::Vault::open(dir);
    if (!vault.ok()) {
        err << "cannot open vault: " << vault.error().toString() << Qt::endl;
        return std::nullopt;
    }
    return std::move(vault).value();
}

} // namespace

int runGamesaveExport(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Write games' backups to a .gamesave file."));
    parser.addHelpOption();
    const QCommandLineOption vaultOption(QStringLiteral("vault"), QStringLiteral("Vault directory."), QStringLiteral("dir"));
    const QCommandLineOption gameOption(QStringLiteral("game"), QStringLiteral("Game id (repeatable). Default: every game."),
                                        QStringLiteral("id"));
    const QCommandLineOption outputOption(QStringLiteral("output"), QStringLiteral("File to write."), QStringLiteral("file"));
    const QCommandLineOption historyOption(QStringLiteral("all-history"),
                                           QStringLiteral("Every snapshot instead of each game's newest."));
    const QCommandLineOption incrementalOption(QStringLiteral("incremental"),
                                               QStringLiteral("Leave out files the previous export already carried."));
    parser.addOptions({vaultOption, gameOption, outputOption, historyOption, incrementalOption});
    if (!parser.parse(arguments) || !parser.isSet(vaultOption) || !parser.isSet(outputOption)) {
        err << (parser.errorText().isEmpty() ? parser.helpText() : parser.errorText()) << Qt::endl;
        return 2;
    }
    auto vault = openVaultAt(parser.value(vaultOption), err);
    if (!vault)
        return 1;

    const CatalogLoad catalog = loadCatalog();
    const services::LocalProfileStore local(localProfilesDirectory());
    QStringList ids = parser.values(gameOption);
    if (ids.isEmpty())
        ids = vault->games();

    services::ExportRequest request;
    request.path = parser.value(outputOption);
    request.allHistory = parser.isSet(historyOption);
    request.appVersion = appVersion();
    request.sourceOs = platform::osName(platform::hostOs());
    for (const QString &id : std::as_const(ids)) {
        const auto entry = catalog.catalog.find(id);
        services::ExportGame game{id, entry ? entry->profile.name : id, std::nullopt};
        if (entry && entry->channel == u"local") {
            if (auto json = local.profileJson(id); json.ok())
                game.localProfile = QJsonDocument::fromJson(json.value()).object();
        }
        request.games.append(game);
    }
    services::ExportLog log(exportLogDirectory());
    if (parser.isSet(incrementalOption)) {
        request.base = log.latest();
        if (!request.base) {
            err << "no previous export to base an incremental export on" << Qt::endl;
            return 1;
        }
    }
    const auto record = services::exportGames(*vault, request);
    if (!record.ok()) {
        err << record.error().toString() << Qt::endl;
        return 1;
    }
    if (auto logged = log.add(record.value()); !logged.ok())
        err << "warning: " << logged.error().toString() << Qt::endl;
    out << "Exported " << record.value().gameIds.size() << " game(s) to " << request.path << " ("
        << record.value().blobs.size() << " file(s)" << (request.base ? ", incremental" : "") << ")" << Qt::endl;
    return 0;
}

int runGamesaveImport(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Add the backups of a .gamesave file to a vault (restores nothing)."));
    parser.addHelpOption();
    const QCommandLineOption vaultOption(QStringLiteral("vault"), QStringLiteral("Vault directory."), QStringLiteral("dir"));
    parser.addOption(vaultOption);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("The .gamesave file."));
    if (!parser.parse(arguments) || !parser.isSet(vaultOption) || parser.positionalArguments().size() != 1) {
        err << (parser.errorText().isEmpty() ? parser.helpText() : parser.errorText()) << Qt::endl;
        return 2;
    }
    auto vault = openVaultAt(parser.value(vaultOption), err);
    if (!vault)
        return 1;
    const auto reader = archive::GamesaveReader::open(parser.positionalArguments().first());
    if (!reader.ok()) {
        err << reader.error().toString() << Qt::endl;
        return 1;
    }
    const CatalogLoad catalog = loadCatalog();
    QSet<QString> known;
    for (const auto &entry : catalog.catalog.entries())
        known.insert(entry.profile.id);
    services::LocalProfileStore local(localProfilesDirectory());
    const auto report = services::importGamesave(*vault, reader.value(), &local, known);
    if (!report.ok()) {
        err << report.error().toString() << Qt::endl;
        return 1;
    }
    out << "Imported " << report.value().imported << " snapshot(s) of " << report.value().games.join(QStringLiteral(", "))
        << "; " << report.value().alreadyPresent << " were already in the vault." << Qt::endl;
    for (const QString &name : report.value().addedLocalProfiles)
        out << "  added the local profile of " << name << Qt::endl;
    out << "Nothing was restored. Use 'snapshots' and 'restore' to bring a backup back." << Qt::endl;
    return 0;
}

} // namespace saveforge::cli
