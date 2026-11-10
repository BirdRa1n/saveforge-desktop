#pragma once

#include "savevault/platform/Processes.h"
#include "savevault/services/GameTarget.h"
#include "savevault/services/Library.h"
#include "savevault/vault/Snapshot.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace savevault::services {

// Notices when games stop running, for backups on game exit (snapshot trigger
// "game-exit"). Feed it the library and the running processes periodically;
// a game counts as running by its process names or by an executable inside
// its installation folder (see isGameRunning()).
class GameExitTracker {
public:
    // Ids of the games that were running at the previous call and are not
    // running now. The first call only records what is running.
    [[nodiscard]] QStringList update(const QList<LibraryGame> &games, const QList<platform::ProcessInfo> &processes);

    // Ids of the games running at the last call.
    [[nodiscard]] const QSet<QString> &running() const { return m_running; }

private:
    QSet<QString> m_running;
};

// Whether the files the profile matches now differ from `last` in their set
// of paths, sizes or modification times. Cheap: compares metadata only, no
// hashing. True when there is no previous snapshot and saves exist; false when
// no file matches (nothing to back up).
[[nodiscard]] bool savesChangedSince(const GameTarget &target, const vault::Snapshot *last);

} // namespace savevault::services
