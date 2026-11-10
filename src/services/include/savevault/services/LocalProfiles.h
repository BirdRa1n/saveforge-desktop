#pragma once

#include "savevault/core/Result.h"
#include "savevault/platform/FolderMap.h"
#include "savevault/platform/Os.h"
#include "savevault/profiles/Profile.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace savevault::services {

// Profiles that exist only on this machine: games the user added by hand and
// profile files the user imported (provenance source "local", see
// savevault-specs/profile/SPEC.md sections 8 and 9). They form the "local"
// catalog channel, which wins over every registry channel.
//
// Each profile is one JSON file, <dir>/<id>.json, in the profile.v1 format,
// so users can back them up or edit them like any other profile. Files are
// validated on every load; invalid ones are skipped with a warning.
class LocalProfileStore {
public:
    struct Load {
        QList<profiles::Profile> profiles;
        QStringList warnings; // "<file>: <reason>" for files that were skipped
    };

    explicit LocalProfileStore(const QString &directory);

    [[nodiscard]] Load load() const;

    // Creates a profile for a game the registry does not know. Each save
    // folder must lie inside one of `userFolders` (e.g. Documents or AppData),
    // because profiles describe locations through placeholders, never through
    // absolute paths; the deepest matching placeholder is used. The id is
    // "local-<name>", made unique. Errors: InvalidArgument for an empty name,
    // no folders, a folder outside the user folders, or a path the profile
    // rules reject (for example a whole Documents folder).
    [[nodiscard]] Result<profiles::Profile> addManualGame(const QString &name, const QStringList &saveFolders,
                                                          const platform::FolderMap &userFolders,
                                                          platform::Os os = platform::hostOs());

    // Validates a profile.v1 JSON file and stores a copy, replacing a local
    // profile with the same id.
    [[nodiscard]] Result<profiles::Profile> importProfile(const QString &file);
    // Same, from profile.v1 JSON already in memory (e.g. from a .gamesave).
    [[nodiscard]] Result<profiles::Profile> importProfileJson(const QByteArray &json);
    // The stored JSON of a local profile, as written (for exports).
    [[nodiscard]] Result<QByteArray> profileJson(const QString &profileId) const;

    // Deletes a local profile. NotFound if there is none with that id.
    [[nodiscard]] Result<void> remove(const QString &profileId);

    [[nodiscard]] const QString &directory() const { return m_dir; }

private:
    [[nodiscard]] Result<void> write(const QString &profileId, const QByteArray &json);
    [[nodiscard]] QString uniqueId(const QString &name) const;

    QString m_dir;
};

} // namespace savevault::services
