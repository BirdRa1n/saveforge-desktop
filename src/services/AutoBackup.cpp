#include "saveforge/services/AutoBackup.h"

#include "saveforge/profiles/PathResolver.h"

#include <QFileInfo>
#include <QHash>
#include <QTimeZone>

#include <cstdlib>

namespace saveforge::services {

QStringList GameExitTracker::update(const QList<LibraryGame> &games, const QList<platform::ProcessInfo> &processes)
{
    QSet<QString> running;
    for (const LibraryGame &game : games) {
        if (isGameRunning(game.target, processes))
            running.insert(game.id());
    }
    QStringList exited;
    for (const QString &id : std::as_const(m_running)) {
        if (!running.contains(id))
            exited.append(id);
    }
    exited.sort();
    m_running = running;
    return exited;
}

bool savesChangedSince(const GameTarget &target, const vault::Snapshot *last)
{
    const QList<profiles::ResolvedFile> files = profiles::PathResolver().resolve(target.profile, target.context);
    if (files.isEmpty())
        return false;
    if (!last || last->files.size() != files.size())
        return true;

    QHash<QString, const vault::SnapshotFile *> previous;
    for (const vault::SnapshotFile &file : last->files)
        previous.insert(file.path, &file);
    for (const profiles::ResolvedFile &file : files) {
        const vault::SnapshotFile *before = previous.value(file.templatePath);
        if (!before)
            return true;
        const QFileInfo info(file.absolutePath);
        if (info.size() != before->size)
            return true;
        // Snapshots keep second precision, and FAT volumes round to 2 s.
        const qint64 delta = info.lastModified(QTimeZone::UTC).toSecsSinceEpoch() - before->mtime.toSecsSinceEpoch();
        if (std::llabs(delta) > 2)
            return true;
    }
    return false;
}

} // namespace saveforge::services
