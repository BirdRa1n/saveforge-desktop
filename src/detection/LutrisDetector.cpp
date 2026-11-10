#include "saveforge/detection/Launchers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace saveforge::detection {

namespace {

QStringList lutrisGameDirs(const DetectionEnvironment &env)
{
    if (env.hostOs != platform::Os::Linux)
        return {};
    QStringList dirs{env.home + QStringLiteral("/.config/lutris/games"),
                     env.home + QStringLiteral("/.local/share/lutris/games"),
                     env.home + QStringLiteral("/.var/app/net.lutris.Lutris/config/lutris/games"),
                     env.home + QStringLiteral("/.var/app/net.lutris.Lutris/data/lutris/games")};
    dirs.removeIf([](const QString &dir) { return !QFileInfo(dir).isDir(); });
    return dirs;
}

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && ((value.startsWith(u'"') && value.endsWith(u'"')) || (value.startsWith(u'\'') && value.endsWith(u'\''))))
        value = value.mid(1, value.size() - 2);
    return value;
}

// The scalar keys of the top-level "game:" section of a Lutris game file.
// Lutris writes plain YAML (`section:` then `  key: value`), which this reads
// without a YAML library; anything more complex is ignored.
QHash<QString, QString> gameSection(const QString &path)
{
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text) || file.size() > 1024 * 1024)
        return values;
    static const QRegularExpression entry(QStringLiteral("^\\s+([A-Za-z_]+):\\s*(.*)$"));
    QTextStream in(&file);
    bool inGame = false;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.trimmed().isEmpty() || line.trimmed().startsWith(u'#'))
            continue;
        if (!line.at(0).isSpace()) {
            inGame = line.trimmed() == u"game:";
            continue;
        }
        if (!inGame)
            continue;
        const auto match = entry.match(line);
        if (match.hasMatch() && !match.captured(2).isEmpty())
            values.insert(match.captured(1), unquote(match.captured(2)));
    }
    return values;
}

// "hollow-knight-1699999999.yml" -> "Hollow Knight"
QString nameFromFile(const QString &fileName)
{
    QString slug = QFileInfo(fileName).completeBaseName();
    static const QRegularExpression stamp(QStringLiteral("-\\d{6,}$"));
    slug.remove(stamp);
    QStringList words = slug.split(u'-', Qt::SkipEmptyParts);
    for (QString &word : words)
        word[0] = word.at(0).toUpper();
    return words.join(u' ');
}

} // namespace

QList<InstallCandidate> detectLutris(const DetectionEnvironment &env)
{
    QList<InstallCandidate> games;
    QSet<QString> seen;
    for (const QString &dir : lutrisGameDirs(env)) {
        for (const QFileInfo &file : QDir(dir).entryInfoList({QStringLiteral("*.yml")}, QDir::Files, QDir::Name)) {
            const QHash<QString, QString> game = gameSection(file.absoluteFilePath());
            const QString exe = QDir::cleanPath(game.value(QStringLiteral("exe")));
            if (exe.isEmpty() || exe == u".")
                continue;
            InstallCandidate candidate;
            candidate.launcher = QStringLiteral("lutris");
            candidate.name = nameFromFile(file.fileName());
            const QString prefix = game.value(QStringLiteral("prefix"));
            const QString workingDir = game.value(QStringLiteral("working_dir"));
            // Relative executables are relative to the prefix or working folder.
            QString exePath = exe;
            if (QDir::isRelativePath(exePath))
                exePath = QDir::cleanPath((workingDir.isEmpty() ? prefix : workingDir) + u'/' + exePath);
            candidate.installDir = QDir::cleanPath(workingDir.isEmpty() ? QFileInfo(exePath).absolutePath() : workingDir);
            if (!QFileInfo(candidate.installDir).isDir() || seen.contains(candidate.installDir))
                continue;
            if (exePath.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
                candidate.gameOs = platform::Os::Windows;
                const QString normalized = normalizeWinePrefix(prefix);
                if (!normalized.isEmpty()) {
                    candidate.winePrefix = normalized;
                    candidate.wineUser = winePrefixUser(normalized);
                }
            } else {
                candidate.gameOs = platform::Os::Linux;
            }
            candidate.executables = findExecutables(candidate.installDir, candidate.gameOs);
            const QString exeName = QFileInfo(exePath).fileName();
            if (!exeName.isEmpty() && !candidate.executables.contains(exeName, Qt::CaseInsensitive))
                candidate.executables.prepend(exeName);
            seen.insert(candidate.installDir);
            games.append(candidate);
        }
    }
    return games;
}

} // namespace saveforge::detection
