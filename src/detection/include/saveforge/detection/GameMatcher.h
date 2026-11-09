#pragma once

#include "saveforge/detection/DetectionEnvironment.h"
#include "saveforge/detection/InstallCandidate.h"
#include "saveforge/profiles/PathResolver.h"
#include "saveforge/profiles/Profile.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace saveforge::detection {

// Why a game was considered present, from most to least reliable.
enum class MatchReason {
    StoreId,         // store id of the installation is listed in the profile
    Executable,      // an executable name in the install dir is unique to one profile
    InstallDirName,  // the install folder or launcher title matches one profile's installDirNames
    SavesFound,      // not installed (or not detected), but the profile's saves exist
};

struct DetectedGame {
    QString profileId;
    MatchReason reason = MatchReason::StoreId;
    std::optional<InstallCandidate> install;   // empty for SavesFound
    qsizetype saveFilesFound = 0;              // set by the save-first scan
};

struct DetectionReport {
    QList<DetectedGame> games;
    QList<InstallCandidate> unmatched;         // installed games without a profile
};

// Matches installations to profiles. Weak signals (executable and folder
// names) only count when they point to exactly one profile; ambiguous matches
// are left unmatched rather than guessed.
class GameMatcher {
public:
    struct Match {
        QString profileId;
        MatchReason reason;
    };

    // `profiles` must already be de-duplicated by precedence (local > official >
    // upstream); on conflicting store ids the first profile wins.
    explicit GameMatcher(const QList<profiles::Profile> &profiles);

    [[nodiscard]] std::optional<Match> match(const InstallCandidate &candidate) const;

    // Lower-case alphanumerics only: "Hollow Knight" and "hollow_knight" are equal.
    [[nodiscard]] static QString normalizeName(const QString &name);

private:
    QHash<QString, QString> m_storeIds;        // "steam:367520" -> profile id
    QHash<QString, QStringList> m_executables; // lower-case file name -> profile ids
    QHash<QString, QStringList> m_dirNames;    // normalized name -> profile ids
};

// Full detection: matches `installs`, then (when `saveFirst`) scans the host
// for saves of every profile that was not matched, so manually installed,
// portable and uninstalled games with saves left behind are found too.
[[nodiscard]] DetectionReport detectGames(const QList<profiles::Profile> &profiles,
                                          const QList<InstallCandidate> &installs,
                                          const DetectionEnvironment &env, bool saveFirst = true);

} // namespace saveforge::detection
