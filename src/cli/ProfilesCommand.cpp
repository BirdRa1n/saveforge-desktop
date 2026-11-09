// `profiles list | add | import | remove`: local profiles and manual games.

#include "CliSupport.h"
#include "Commands.h"

#include "saveforge/detection/DetectionEnvironment.h"
#include "saveforge/services/LocalProfiles.h"

#include <QCommandLineParser>

namespace saveforge::cli {

namespace {

int printError(const Error &error, QTextStream &err)
{
    err << error.toString() << Qt::endl;
    return 1;
}

int runList(QTextStream &out, QTextStream &err)
{
    const services::LocalProfileStore store(localProfilesDirectory());
    const services::LocalProfileStore::Load load = store.load();
    printWarnings(load.warnings, err);
    for (const profiles::Profile &profile : load.profiles) {
        out << profile.id << "  " << profile.name << Qt::endl;
        for (const profiles::SaveRule &rule : profile.saves) {
            for (const QString &path : rule.paths)
                out << "    " << path << Qt::endl;
        }
    }
    out << load.profiles.size() << " local profile(s) in " << store.directory() << Qt::endl;
    return 0;
}

int runAdd(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Add a game the registry does not know, from its save folders."));
    parser.addHelpOption();
    const QCommandLineOption nameOption(QStringLiteral("name"), QStringLiteral("Game name."), QStringLiteral("name"));
    const QCommandLineOption folderOption(QStringLiteral("folder"),
                                          QStringLiteral("Save folder inside your user folders (repeatable)."),
                                          QStringLiteral("dir"));
    parser.addOption(nameOption);
    parser.addOption(folderOption);
    if (!parser.parse(arguments) || !parser.isSet(nameOption) || !parser.isSet(folderOption)) {
        err << (parser.errorText().isEmpty() ? parser.helpText() : parser.errorText()) << Qt::endl;
        return 2;
    }
    services::LocalProfileStore store(localProfilesDirectory());
    const auto added = store.addManualGame(parser.value(nameOption), parser.values(folderOption),
                                           detection::DetectionEnvironment::host().userFolders);
    if (!added.ok())
        return printError(added.error(), err);
    out << "Added " << added.value().id << " (" << added.value().name << ")" << Qt::endl;
    for (const QString &path : added.value().saves.first().paths)
        out << "    " << path << Qt::endl;
    return 0;
}

int runImport(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    if (arguments.size() != 2) {
        err << "Usage: saveforge-cli profiles import <profile.json>" << Qt::endl;
        return 2;
    }
    services::LocalProfileStore store(localProfilesDirectory());
    const auto imported = store.importProfile(arguments.at(1));
    if (!imported.ok())
        return printError(imported.error(), err);
    out << "Imported " << imported.value().id << " (" << imported.value().name << ")" << Qt::endl;
    return 0;
}

int runRemove(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    if (arguments.size() != 2) {
        err << "Usage: saveforge-cli profiles remove <id>" << Qt::endl;
        return 2;
    }
    services::LocalProfileStore store(localProfilesDirectory());
    if (auto removed = store.remove(arguments.at(1)); !removed.ok())
        return printError(removed.error(), err);
    out << "Removed " << arguments.at(1) << ". Its backups stay in the vault." << Qt::endl;
    return 0;
}

} // namespace

int runProfiles(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    const QString sub = arguments.value(1);
    const QStringList rest = QStringList{arguments.value(0)} + arguments.mid(2);
    if (sub == u"list")
        return runList(out, err);
    if (sub == u"add")
        return runAdd(rest, out, err);
    if (sub == u"import")
        return runImport(rest, out, err);
    if (sub == u"remove")
        return runRemove(rest, out, err);
    err << "Usage: saveforge-cli profiles list\n"
           "       saveforge-cli profiles add --name <name> --folder <dir>...\n"
           "       saveforge-cli profiles import <profile.json>\n"
           "       saveforge-cli profiles remove <id>\n";
    return 2;
}

} // namespace saveforge::cli
