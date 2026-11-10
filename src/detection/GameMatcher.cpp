#include "savevault/detection/GameMatcher.h"

#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace savevault::detection {

namespace {

QString storeKey(const QString &store, const QString &id)
{
    return store + u':' + id;
}

void addUnique(QHash<QString, QStringList> &index, const QString &key, const QString &profileId)
{
    if (key.isEmpty())
        return;
    QStringList &ids = index[key];
    if (!ids.contains(profileId))
        ids.append(profileId);
}

// The single profile all `keys` agree on, or empty when none or several match.
QString uniqueProfile(const QHash<QString, QStringList> &index, const QStringList &keys)
{
    QSet<QString> found;
    for (const QString &key : keys) {
        for (const QString &id : index.value(key))
            found.insert(id);
    }
    return found.size() == 1 ? *found.cbegin() : QString();
}

} // namespace

QString GameMatcher::normalizeName(const QString &name)
{
    QString normalized;
    normalized.reserve(name.size());
    for (QChar c : name) {
        if (c.isLetterOrNumber())
            normalized += c.toLower();
    }
    return normalized;
}

GameMatcher::GameMatcher(const QList<profiles::Profile> &profiles)
{
    const auto addStoreId = [this](const QString &key, const QString &profileId) {
        if (!m_storeIds.contains(key)) // on conflicts the first (highest-precedence) profile wins
            m_storeIds.insert(key, profileId);
    };
    for (const profiles::Profile &profile : profiles) {
        const profiles::Identifiers &ids = profile.identifiers;
        for (qint64 id : ids.steam)
            addStoreId(storeKey(QStringLiteral("steam"), QString::number(id)), profile.id);
        for (qint64 id : ids.gog)
            addStoreId(storeKey(QStringLiteral("gog"), QString::number(id)), profile.id);
        for (const QString &id : ids.epic)
            addStoreId(storeKey(QStringLiteral("epic"), id), profile.id);
        for (const QString &exe : ids.executables)
            addUnique(m_executables, exe.toLower(), profile.id);
        for (const QString &dir : ids.installDirNames)
            addUnique(m_dirNames, normalizeName(dir), profile.id);
    }
}

std::optional<GameMatcher::Match> GameMatcher::match(const InstallCandidate &candidate) const
{
    if (const QString id = m_storeIds.value(storeKey(candidate.store, candidate.storeId)); !id.isEmpty())
        return Match{id, MatchReason::StoreId};

    QStringList exes;
    for (const QString &exe : candidate.executables)
        exes.append(exe.toLower());
    if (const QString id = uniqueProfile(m_executables, exes); !id.isEmpty())
        return Match{id, MatchReason::Executable};

    QStringList names{normalizeName(QFileInfo(candidate.installDir).fileName())};
    if (!candidate.name.isEmpty())
        names.append(normalizeName(candidate.name));
    if (const QString id = uniqueProfile(m_dirNames, names); !id.isEmpty())
        return Match{id, MatchReason::InstallDirName};

    return std::nullopt;
}

DetectionReport detectGames(const QList<profiles::Profile> &profiles, const QList<InstallCandidate> &installs,
                            const DetectionEnvironment &env, bool saveFirst)
{
    DetectionReport report;
    const GameMatcher matcher(profiles);
    QSet<QString> detected;
    QStringList storeUserIds;

    for (const InstallCandidate &candidate : installs) {
        for (const QString &id : candidate.storeUserIds) {
            if (!storeUserIds.contains(id))
                storeUserIds.append(id);
        }
        if (const auto match = matcher.match(candidate)) {
            report.games.append(DetectedGame{match->profileId, match->reason, candidate, 0});
            detected.insert(match->profileId);
        } else {
            report.unmatched.append(candidate);
        }
    }

    if (!saveFirst)
        return report;

    const profiles::PathResolver resolver;
    const profiles::ResolveContext context = hostResolveContext(env, storeUserIds);
    QList<DetectedGame> savesOnly;
    for (const profiles::Profile &profile : profiles) {
        if (detected.contains(profile.id))
            continue;
        const qsizetype found = resolver.resolve(profile, context).size();
        if (found > 0) {
            savesOnly.append(DetectedGame{profile.id, MatchReason::SavesFound, std::nullopt, found});
            detected.insert(profile.id);
        }
    }
    std::sort(savesOnly.begin(), savesOnly.end(),
              [](const DetectedGame &a, const DetectedGame &b) { return a.profileId < b.profileId; });
    report.games.append(savesOnly);
    return report;
}

} // namespace savevault::detection
