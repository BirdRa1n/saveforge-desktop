#include "saveforge/registry/ProfileCatalog.h"

#include <QTest>

using namespace saveforge;
using namespace saveforge::registry;

namespace {

profiles::Profile profile(const QString &id, const QString &source, QList<qint64> steam = {},
                          const QString &basedOn = {}, const QString &name = {})
{
    profiles::Profile p;
    p.id = id;
    p.name = name.isEmpty() ? id : name;
    p.identifiers.steam = steam;
    p.provenance.source = source;
    p.provenance.basedOnLudusavi = basedOn;
    return p;
}

QStringList ids(const ProfileCatalog &catalog)
{
    QStringList result;
    for (const CatalogEntry &e : catalog.entries())
        result.append(e.channel + u'/' + e.profile.id);
    return result;
}

} // namespace

class TestProfileCatalog : public QObject {
    Q_OBJECT

private slots:
    void higherChannelsReplaceLowerOnes()
    {
        ProfileCatalog catalog;
        // Added in reverse precedence order on purpose.
        catalog.addChannel(QStringLiteral("upstream-ludusavi"),
                           {profile(QStringLiteral("hollow-knight"), QStringLiteral("ludusavi"), {367520}, {}, QStringLiteral("Hollow Knight")),
                            profile(QStringLiteral("celeste"), QStringLiteral("ludusavi"), {504230}, {}, QStringLiteral("Celeste")),
                            profile(QStringLiteral("hades"), QStringLiteral("ludusavi"), {1145360}, {}, QStringLiteral("Hades")),
                            profile(QStringLiteral("terraria"), QStringLiteral("ludusavi"), {105600}, {}, QStringLiteral("Terraria"))});
        // Official profile with another id but based on the same Ludusavi entry.
        catalog.addChannel(QStringLiteral("official"),
                           {profile(QStringLiteral("hollow-knight-official"), QStringLiteral("saveforge"), {}, QStringLiteral("Hollow Knight"))});
        // Local profile sharing a Steam id with an upstream one.
        catalog.addChannel(QStringLiteral("local"), {profile(QStringLiteral("my-celeste"), QStringLiteral("local"), {504230})});
        // Same id in a lower channel.
        catalog.addChannel(QStringLiteral("official"), {profile(QStringLiteral("hades"), QStringLiteral("saveforge"))});

        QCOMPARE(ids(catalog), (QStringList{QStringLiteral("local/my-celeste"), QStringLiteral("official/hollow-knight-official"),
                                            QStringLiteral("official/hades"), QStringLiteral("upstream-ludusavi/terraria")}));
        QCOMPARE(catalog.find(QStringLiteral("hades"))->channel, QStringLiteral("official"));
        QVERIFY(!catalog.find(QStringLiteral("celeste")).has_value());
    }

    void keepsDistinctProfilesWithinOneChannel()
    {
        // Upstream entries that share a Steam id are different games/editions: keep both.
        ProfileCatalog catalog;
        catalog.addChannel(QStringLiteral("upstream-ludusavi"),
                           {profile(QStringLiteral("game"), QStringLiteral("ludusavi"), {1}),
                            profile(QStringLiteral("game-demo"), QStringLiteral("ludusavi"), {1})});
        QCOMPARE(catalog.size(), 2);
    }

    void ranksChannels()
    {
        QVERIFY(ProfileCatalog::channelRank(QStringLiteral("local")) < ProfileCatalog::channelRank(QStringLiteral("official")));
        QVERIFY(ProfileCatalog::channelRank(QStringLiteral("official"))
                < ProfileCatalog::channelRank(QStringLiteral("upstream-ludusavi")));
        QVERIFY(ProfileCatalog::channelRank(QStringLiteral("upstream-ludusavi"))
                < ProfileCatalog::channelRank(QStringLiteral("something-new")));
    }
};

QTEST_GUILESS_MAIN(TestProfileCatalog)
#include "tst_profile_catalog.moc"
