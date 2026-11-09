#pragma once

#include "saveforge/platform/FolderMap.h"
#include "saveforge/platform/Os.h"
#include "saveforge/profiles/PathRules.h"
#include "saveforge/profiles/Profile.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace saveforge::profiles {

// Where and how a profile is resolved on this machine.
struct ResolveContext {
    platform::Os gameOs = platform::hostOs();   // platform of the game build
    QString store;                              // store that installed the game; empty if unknown
    platform::FolderMap folders;                // root placeholders (host folders, or a Wine prefix + "base")
    bool caseInsensitive = platform::hostOs() != platform::Os::Linux;
    QStringList storeUserIds;                   // values for <storeUserId>; each one expands the path
    QString storeGameId;                        // value for <storeGameId>
    QString osUserName;                         // value for <osUserName>
};

// A file on disk selected by a profile.
struct ResolvedFile {
    QString absolutePath;
    // Concrete path in template form, as stored in snapshots: the root
    // placeholder, literal segments, and segment placeholders whose values are
    // recorded in `bindings`.
    QString templatePath;
    QString ruleId;
    QList<Tag> tags;
    QHash<QString, QString> bindings;
};

// Expands a profile's save rules into concrete files
// (saveforge-specs/profile/SPEC.md section 4 and path-rules.md run-time obligations).
//
// - Rules whose `when` does not match the context, or whose root placeholder is
//   not available, are skipped silently.
// - Directory matches are expanded recursively; symbolic links and junctions are
//   never followed or returned.
// - Files in sensitive locations (rule P6 applied to the real path) and files
//   matching `exclude` are dropped.
// - Mechanism `latest-n` keeps only the n newest matches of each path.
// The result is sorted by absolute path and contains each file once.
class PathResolver {
public:
    explicit PathResolver(const PathRules &rules = PathRules::builtin());

    [[nodiscard]] QList<ResolvedFile> resolve(const Profile &profile, const ResolveContext &context) const;

    // Whether a save rule applies to the context (OS and store filters).
    [[nodiscard]] bool ruleApplies(const SaveRule &rule, const QString &path, const ResolveContext &context) const;

private:
    const PathRules &m_rules;
};

} // namespace saveforge::profiles
