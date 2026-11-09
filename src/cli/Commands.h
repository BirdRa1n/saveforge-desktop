#pragma once

#include <QStringList>
#include <QTextStream>

namespace saveforge::cli {

// `saveforge-cli detect [--profiles <dir>] [--no-save-scan]`
// Lists games installed by supported launchers. With --profiles, also matches
// them to the profile JSON files in <dir> and scans for saves of the rest.
// Read-only. Returns the process exit code.
int runDetect(const QStringList &arguments, QTextStream &out, QTextStream &err);

} // namespace saveforge::cli
