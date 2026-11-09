#include "saveforge/services/GameTarget.h"

#include <QDir>

namespace saveforge::services {

bool isGameRunning(const GameTarget &target, const QList<platform::ProcessInfo> &processes)
{
    const QStringList &names = target.profile.backup.processNames;
    QString installDir = target.context.folders.value(QStringLiteral("base"));
    if (!installDir.isEmpty())
        installDir = QDir::cleanPath(installDir) + u'/';

    for (const platform::ProcessInfo &process : processes) {
        if (names.contains(process.name, Qt::CaseInsensitive))
            return true;
        if (!installDir.isEmpty() && !process.executablePath.isEmpty()
            && QDir::cleanPath(process.executablePath).startsWith(installDir, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace saveforge::services
