#pragma once

#include <QStringList>
#include <QTextStream>

namespace saveforge::cli {

// `saveforge-cli detect [--profiles <dir>] [--no-save-scan]`
// Lists games installed by supported launchers. With --profiles, also matches
// them to the profile JSON files in <dir> and scans for saves of the rest.
// Read-only. Returns the process exit code.
int runDetect(const QStringList &arguments, QTextStream &out, QTextStream &err);

// `saveforge-cli backup --vault <dir> (--game <id> | --profile <file>) [--label <text>] [--map <placeholder>=<dir>]...`
int runBackup(const QStringList &arguments, QTextStream &out, QTextStream &err);

// `saveforge-cli snapshots --vault <dir> [--game <id>]`
int runSnapshots(const QStringList &arguments, QTextStream &out, QTextStream &err);

// `saveforge-cli restore --vault <dir> (--game <id> | --profile <file>) --snapshot <id> [--cloud <policy>] [--yes]`
// Prints the plan; writes files only with --yes.
int runRestore(const QStringList &arguments, QTextStream &out, QTextStream &err);

// `saveforge-cli export --vault <dir> --output <file> [--game <id>]... [--all-history] [--incremental]`
int runGamesaveExport(const QStringList &arguments, QTextStream &out, QTextStream &err);

// `saveforge-cli import --vault <dir> <file.gamesave>`: adds backups, restores nothing.
int runGamesaveImport(const QStringList &arguments, QTextStream &out, QTextStream &err);

// `saveforge-cli profiles list | add --name <name> --folder <dir>... | import <file> | remove <id>`
// Manages local profiles: games added by hand and imported profile files.
int runProfiles(const QStringList &arguments, QTextStream &out, QTextStream &err);

// `saveforge-cli registry update | import <dir> [--channel <name>] | status`
int runRegistry(const QStringList &arguments, QTextStream &out, QTextStream &err);

} // namespace saveforge::cli
