#include "savevault/registry/ProfileCatalog.h"

#include <QSet>

#include <algorithm>

namespace savevault::registry {

namespace {

// Keys that identify the game a profile describes.
QStringList gameKeys(const profiles::Profile &profile)
{
    QStringList keys{QStringLiteral("id:") + profile.id};
    const QString ludusavi = profile.provenance.basedOnLudusavi.isEmpty() && profile.provenance.source == u"ludusavi"
                                 ? profile.name
                                 : profile.provenance.basedOnLudusavi;
    if (!ludusavi.isEmpty())
        keys.append(QStringLiteral("ludusavi:") + ludusavi);
    for (qint64 id : profile.identifiers.steam)
        keys.append(QStringLiteral("steam:") + QString::number(id));
    for (qint64 id : profile.identifiers.gog)
        keys.append(QStringLiteral("gog:") + QString::number(id));
    for (const QString &id : profile.identifiers.epic)
        keys.append(QStringLiteral("epic:") + id);
    return keys;
}

} // namespace

int ProfileCatalog::channelRank(const QString &channel)
{
    static const QStringList order{QStringLiteral("local"), QStringLiteral("official"),
                                   QStringLiteral("community-experimental"), QStringLiteral("upstream-ludusavi")};
    const qsizetype rank = order.indexOf(channel);
    return rank < 0 ? static_cast<int>(order.size()) : static_cast<int>(rank);
}

void ProfileCatalog::addChannel(const QString &channel, const QList<profiles::Profile> &profiles)
{
    m_channels[channel] += profiles;
}

QList<CatalogEntry> ProfileCatalog::entries() const
{
    QStringList channels = m_channels.keys();
    std::sort(channels.begin(), channels.end(), [](const QString &a, const QString &b) {
        const int ra = channelRank(a);
        const int rb = channelRank(b);
        return ra != rb ? ra < rb : a < b;
    });

    QList<CatalogEntry> result;
    QSet<QString> claimed; // keys of games described by stronger channels
    for (const QString &channel : std::as_const(channels)) {
        QSet<QString> claimedHere;
        QSet<QString> idsHere;
        for (const profiles::Profile &profile : m_channels.value(channel)) {
            const QStringList keys = gameKeys(profile);
            if (idsHere.contains(profile.id)
                || std::any_of(keys.begin(), keys.end(), [&claimed](const QString &k) { return claimed.contains(k); }))
                continue;
            idsHere.insert(profile.id);
            for (const QString &key : keys)
                claimedHere.insert(key);
            result.append({profile, channel});
        }
        claimed.unite(claimedHere);
    }
    return result;
}

QList<profiles::Profile> ProfileCatalog::profiles() const
{
    QList<profiles::Profile> list;
    for (const CatalogEntry &entry : entries())
        list.append(entry.profile);
    return list;
}

std::optional<CatalogEntry> ProfileCatalog::find(const QString &profileId) const
{
    for (const CatalogEntry &entry : entries()) {
        if (entry.profile.id == profileId)
            return entry;
    }
    return std::nullopt;
}

} // namespace savevault::registry
