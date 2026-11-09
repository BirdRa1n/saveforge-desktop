#pragma once

#include "saveforge/profiles/Profile.h"

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace saveforge::registry {

struct CatalogEntry {
    profiles::Profile profile;
    QString channel;                       // "local", "official", "community-experimental", "upstream-ludusavi"
};

// All profiles available on this machine, one per game
// (saveforge-specs/profile/SPEC.md section 9).
//
// Channels are applied in precedence order: local > official >
// community-experimental > upstream-ludusavi. A profile is hidden when a
// higher-precedence channel already describes the same game: equal id, the
// same Ludusavi entry (provenance.basedOn.ludusavi or its name), or a shared
// store id. The winner replaces the others entirely; profiles are never merged.
class ProfileCatalog {
public:
    // Lower is stronger; unknown channels rank last.
    [[nodiscard]] static int channelRank(const QString &channel);

    // Adds a channel's profiles. Order of calls does not matter.
    void addChannel(const QString &channel, const QList<profiles::Profile> &profiles);

    [[nodiscard]] QList<CatalogEntry> entries() const;
    [[nodiscard]] QList<profiles::Profile> profiles() const;
    [[nodiscard]] std::optional<CatalogEntry> find(const QString &profileId) const;
    [[nodiscard]] qsizetype size() const { return entries().size(); }

private:
    QHash<QString, QList<profiles::Profile>> m_channels;
};

} // namespace saveforge::registry
