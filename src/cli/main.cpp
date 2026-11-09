#include "Commands.h"
#include "saveforge/core/Version.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cstdio>

// saveforge-cli: command-line interface for scripts and end-to-end tests.
//
//   saveforge-cli --version
//   saveforge-cli detect [--profiles <dir>] [--no-save-scan]
//   saveforge-cli registry update | registry import <dir> | registry status
//   saveforge-cli profiles list | add --name <name> --folder <dir> | import <file> | remove <id>
//   saveforge-cli backup --vault <dir> (--game <id> | --profile <file>) [--label <text>]
//   saveforge-cli snapshots --vault <dir> [--game <id>]
//   saveforge-cli restore --vault <dir> (--game <id> | --profile <file>) --snapshot <id> [--cloud <policy>] [--yes]
namespace {

void printUsage(QTextStream &out)
{
    out << "Usage: saveforge-cli <command> [options]\n\n"
           "Commands:\n"
           "  detect     List installed games and match them to profiles (read-only)\n"
           "  registry   Download or import signed profiles, or show the cache status\n"
           "  profiles   Add games by hand and manage local profiles\n"
           "  backup    Back up a game into a vault\n"
           "  snapshots  List the snapshots in a vault\n"
           "  restore    Plan a restore, and perform it with --yes\n\n"
           "Options:\n"
           "  -h, --help     Show this help (use '<command> --help' for command options)\n"
           "  -v, --version  Show the version\n";
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SaveForge"));
    QCoreApplication::setApplicationName(QStringLiteral("saveforge-cli"));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(saveforge::kVersion.data(),
                                                              static_cast<qsizetype>(saveforge::kVersion.size())));

    QTextStream out(stdout);
    QTextStream err(stderr);
    const QStringList args = QCoreApplication::arguments();

    if (args.size() < 2 || args.at(1) == u"-h" || args.at(1) == u"--help") {
        printUsage(out);
        return args.size() < 2 ? 2 : 0;
    }
    if (args.at(1) == u"-v" || args.at(1) == u"--version") {
        out << QCoreApplication::applicationName() << ' ' << QCoreApplication::applicationVersion() << Qt::endl;
        return 0;
    }

    // Each command parses its own options; argv[0] is kept for help output.
    const QString command = args.at(1);
    const QStringList commandArgs = QStringList{args.at(0)} + args.mid(2);
    if (command == u"detect")
        return saveforge::cli::runDetect(commandArgs, out, err);
    if (command == u"backup")
        return saveforge::cli::runBackup(commandArgs, out, err);
    if (command == u"snapshots")
        return saveforge::cli::runSnapshots(commandArgs, out, err);
    if (command == u"restore")
        return saveforge::cli::runRestore(commandArgs, out, err);
    if (command == u"profiles")
        return saveforge::cli::runProfiles(commandArgs, out, err);
    if (command == u"registry")
        return saveforge::cli::runRegistry(commandArgs, out, err);

    err << "unknown command: " << command << "\n\n";
    printUsage(err);
    return 2;
}
