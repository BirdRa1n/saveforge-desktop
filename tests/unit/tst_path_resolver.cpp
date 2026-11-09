#include "saveforge/profiles/PathResolver.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

using namespace saveforge;
using namespace saveforge::profiles;
using saveforge::platform::Os;

namespace {

// Creates `relativePath` (and parent directories) under `root`.
void touch(const QString &root, const QString &relativePath, const QDateTime &mtime = {})
{
    const QString path = root + u'/' + relativePath;
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(relativePath.toUtf8());
    if (mtime.isValid())
        QVERIFY(file.setFileTime(mtime, QFileDevice::FileModificationTime));
}

Profile makeProfile(const QList<SaveRule> &saves)
{
    Profile profile;
    profile.id = QStringLiteral("test-game");
    profile.name = QStringLiteral("Test Game");
    profile.saves = saves;
    profile.mechanism.type = QStringLiteral("standard");
    return profile;
}

SaveRule rule(const QString &id, const QStringList &paths, std::optional<Os> os = std::nullopt)
{
    SaveRule r;
    r.id = id;
    r.tags = {Tag::Save};
    r.when.os = os;
    r.paths = paths;
    return r;
}

ResolveContext contextFor(const QString &root, Os gameOs = Os::Windows)
{
    ResolveContext context;
    context.gameOs = gameOs;
    context.caseInsensitive = true;
    for (const char *name : {"home", "winAppData", "winLocalAppDataLow", "winDocuments", "xdgConfig", "base"}) {
        const QString dir = root + u'/' + QLatin1String(name);
        QDir().mkpath(dir);
        context.folders.set(QLatin1String(name), dir);
    }
    return context;
}

QStringList templatePaths(const QList<ResolvedFile> &files)
{
    QStringList paths;
    for (const ResolvedFile &file : files)
        paths.append(file.templatePath);
    return paths;
}

} // namespace

class TestPathResolver : public QObject {
    Q_OBJECT

private slots:
    void matchesFileGlobs()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        const QString game = tmp.path() + QStringLiteral("/winLocalAppDataLow");
        touch(game, QStringLiteral("Team Cherry/Hollow Knight/user1.dat"));
        touch(game, QStringLiteral("Team Cherry/Hollow Knight/user2.dat"));
        touch(game, QStringLiteral("Team Cherry/Hollow Knight/shared.dat"));

        const auto files = PathResolver().resolve(
            makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<winLocalAppDataLow>/Team Cherry/Hollow Knight/user*.dat")})}),
            ctx);

        QCOMPARE(templatePaths(files), (QStringList{
            QStringLiteral("<winLocalAppDataLow>/Team Cherry/Hollow Knight/user1.dat"),
            QStringLiteral("<winLocalAppDataLow>/Team Cherry/Hollow Knight/user2.dat")}));
        QCOMPARE(files.first().ruleId, QStringLiteral("progress"));
        QCOMPARE(files.first().absolutePath, game + QStringLiteral("/Team Cherry/Hollow Knight/user1.dat"));
    }

    void directoryMatchesAreRecursive()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        touch(tmp.path() + QStringLiteral("/winDocuments"), QStringLiteral("My Games/Game/saves/slot1/data.bin"));
        touch(tmp.path() + QStringLiteral("/winDocuments"), QStringLiteral("My Games/Game/saves/slot2.sav"));
        touch(tmp.path() + QStringLiteral("/winDocuments"), QStringLiteral("My Games/Game/settings.ini"));

        const auto files = PathResolver().resolve(
            makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<winDocuments>/My Games/Game/saves")})}), ctx);
        QCOMPARE(templatePaths(files), (QStringList{
            QStringLiteral("<winDocuments>/My Games/Game/saves/slot1/data.bin"),
            QStringLiteral("<winDocuments>/My Games/Game/saves/slot2.sav")}));
    }

    void doubleStarMatchesAnyDepth()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        touch(tmp.path() + QStringLiteral("/base"), QStringLiteral("saves/a.sav"));
        touch(tmp.path() + QStringLiteral("/base"), QStringLiteral("saves/deep/er/b.sav"));
        touch(tmp.path() + QStringLiteral("/base"), QStringLiteral("saves/deep/c.txt"));

        const auto files = PathResolver().resolve(
            makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<base>/saves/**/*.sav")})}), ctx);
        QCOMPARE(templatePaths(files), (QStringList{
            QStringLiteral("<base>/saves/a.sav"), QStringLiteral("<base>/saves/deep/er/b.sav")}));
    }

    void caseSensitivityFollowsContext()
    {
        QTemporaryDir tmp;
        ResolveContext ctx = contextFor(tmp.path());
        touch(tmp.path() + QStringLiteral("/winAppData"), QStringLiteral("Studio/Game/Save.DAT"));
        const Profile profile = makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<winAppData>/studio/game/*.dat")})});

        QCOMPARE(PathResolver().resolve(profile, ctx).size(), 1);
        // The real on-disk names are recorded, not the pattern's spelling.
        QCOMPARE(PathResolver().resolve(profile, ctx).first().templatePath, QStringLiteral("<winAppData>/Studio/Game/Save.DAT"));

        ctx.caseInsensitive = false;
        QVERIFY(PathResolver().resolve(profile, ctx).isEmpty());
    }

    void expandsStoreUserIdPerAccount()
    {
        QTemporaryDir tmp;
        ResolveContext ctx = contextFor(tmp.path());
        ctx.storeUserIds = {QStringLiteral("111"), QStringLiteral("222"), QStringLiteral("333")};
        touch(tmp.path() + QStringLiteral("/winAppData"), QStringLiteral("Studio/Game/111/slot.sl2"));
        touch(tmp.path() + QStringLiteral("/winAppData"), QStringLiteral("Studio/Game/222/slot.sl2"));

        const auto files = PathResolver().resolve(
            makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<winAppData>/Studio/Game/<storeUserId>/*.sl2")})}), ctx);
        QCOMPARE(files.size(), 2);
        QCOMPARE(files.at(0).templatePath, QStringLiteral("<winAppData>/Studio/Game/<storeUserId>/slot.sl2"));
        QCOMPARE(files.at(0).bindings.value(QStringLiteral("storeUserId")), QStringLiteral("111"));
        QCOMPARE(files.at(1).bindings.value(QStringLiteral("storeUserId")), QStringLiteral("222"));

        ctx.storeUserIds.clear();
        QVERIFY(PathResolver().resolve(
            makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<winAppData>/Studio/Game/<storeUserId>/*.sl2")})}), ctx)
                    .isEmpty());
    }

    void placeholderInsideSegmentIsRestored()
    {
        QTemporaryDir tmp;
        ResolveContext ctx = contextFor(tmp.path());
        ctx.storeUserIds = {QStringLiteral("42")};
        touch(tmp.path() + QStringLiteral("/winDocuments"), QStringLiteral("Game/save_42_1.dat"));

        const auto files = PathResolver().resolve(
            makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<winDocuments>/Game/save_<storeUserId>_?.dat")})}), ctx);
        QCOMPARE(templatePaths(files), QStringList{QStringLiteral("<winDocuments>/Game/save_<storeUserId>_1.dat")});
    }

    void skipsRulesForOtherPlatformsAndStores()
    {
        QTemporaryDir tmp;
        ResolveContext ctx = contextFor(tmp.path(), Os::Linux);
        ctx.store = QStringLiteral("gog");
        touch(tmp.path() + QStringLiteral("/xdgConfig"), QStringLiteral("Game/linux.sav"));
        touch(tmp.path() + QStringLiteral("/winAppData"), QStringLiteral("Game/windows.sav"));
        touch(tmp.path() + QStringLiteral("/home"), QStringLiteral(".game/steam-only.sav"));

        SaveRule steamOnly = rule(QStringLiteral("progress"), {QStringLiteral("<home>/.game/steam-only.sav")});
        steamOnly.when.store = QStringLiteral("steam");
        const Profile profile = makeProfile({
            rule(QStringLiteral("progress"), {QStringLiteral("<xdgConfig>/Game/linux.sav")}),
            rule(QStringLiteral("progress"), {QStringLiteral("<winAppData>/Game/windows.sav")}),  // implied windows
            rule(QStringLiteral("progress"), {QStringLiteral("<home>/Game/x.sav")}, Os::Windows), // explicit windows
            steamOnly,
        });

        QCOMPARE(templatePaths(PathResolver().resolve(profile, ctx)), QStringList{QStringLiteral("<xdgConfig>/Game/linux.sav")});
    }

    void skipsUnavailableRootPlaceholders()
    {
        QTemporaryDir tmp;
        ResolveContext ctx = contextFor(tmp.path());
        ctx.folders.remove(QStringLiteral("base")); // game not installed
        touch(tmp.path() + QStringLiteral("/base"), QStringLiteral("saves/a.sav"));
        QVERIFY(PathResolver().resolve(makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<base>/saves")})}), ctx).isEmpty());
    }

    void appliesExcludeRules()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        const QString docs = tmp.path() + QStringLiteral("/winDocuments");
        touch(docs, QStringLiteral("Game/saves/a.sav"));
        touch(docs, QStringLiteral("Game/saves/debug.log"));
        touch(docs, QStringLiteral("Game/saves/Crashes/dump.dmp"));

        Profile profile = makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<winDocuments>/Game/saves")})});
        profile.exclude = {QStringLiteral("*.log"), QStringLiteral("**/Crashes/**")};
        QCOMPARE(templatePaths(PathResolver().resolve(profile, ctx)), QStringList{QStringLiteral("<winDocuments>/Game/saves/a.sav")});
    }

    void dropsSensitiveFilesFoundAtRunTime()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        touch(tmp.path() + QStringLiteral("/home"), QStringLiteral("Games/Studio/save.dat"));
        touch(tmp.path() + QStringLiteral("/home"), QStringLiteral("Games/.ssh/id_rsa"));

        const auto files = PathResolver().resolve(
            makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<home>/Games/**")})}), ctx);
        QCOMPARE(templatePaths(files), QStringList{QStringLiteral("<home>/Games/Studio/save.dat")});
    }

    void ignoresUnsafePaths()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        touch(tmp.path() + QStringLiteral("/home"), QStringLiteral("anything.txt"));
        // Hand-built profiles bypass the parser; the resolver must still refuse them.
        QVERIFY(PathResolver().resolve(makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<home>/**")})}), ctx).isEmpty());
    }

    void neverFollowsSymbolicLinks()
    {
        QTemporaryDir tmp;
        QTemporaryDir outside;
        const ResolveContext ctx = contextFor(tmp.path());
        touch(outside.path(), QStringLiteral("secret.txt"));
        touch(tmp.path() + QStringLiteral("/winDocuments"), QStringLiteral("Game/real.sav"));
        const QString link = tmp.path() + QStringLiteral("/winDocuments/Game/escape");
#if defined(Q_OS_WIN)
        // QFile::link creates .lnk shortcuts on Windows; a junction is the real
        // threat and can be created without administrator rights.
        QProcess mklink;
        mklink.setStandardOutputFile(QProcess::nullDevice());
        mklink.start(QStringLiteral("cmd"), {QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"),
                                             QDir::toNativeSeparators(link), QDir::toNativeSeparators(outside.path())});
        mklink.waitForFinished();
        if (!QFileInfo(link).isJunction())
            QSKIP("cannot create junctions on this system");
#else
        if (!QFile::link(outside.path(), link) || !QFileInfo(link).isSymLink())
            QSKIP("cannot create symbolic links on this system");
#endif

        const auto files = PathResolver().resolve(
            makeProfile({rule(QStringLiteral("progress"), {QStringLiteral("<winDocuments>/Game")})}), ctx);
        QCOMPARE(templatePaths(files), QStringList{QStringLiteral("<winDocuments>/Game/real.sav")});
    }

    void restoreTargetsResolveOnThisMachine()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        const auto target = resolveRestoreTarget(QStringLiteral("<winAppData>/Studio/Game/<storeUserId>/slot.sl2"),
                                                 {{QStringLiteral("storeUserId"), QStringLiteral("42")}}, ctx);
        QVERIFY(target.ok());
        QCOMPARE(target.value(), tmp.path() + QStringLiteral("/winAppData/Studio/Game/42/slot.sl2"));
    }

    void restoreTargetsReuseExistingSpelling()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        touch(tmp.path() + QStringLiteral("/winAppData"), QStringLiteral("Studio/GAME/old.sav"));
        const auto target = resolveRestoreTarget(QStringLiteral("<winAppData>/Studio/Game/new.sav"), {}, ctx);
        QVERIFY(target.ok());
        QCOMPARE(target.value(), tmp.path() + QStringLiteral("/winAppData/Studio/GAME/new.sav"));
    }

    void restoreTargetsRejectUnsafeInput_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("binding");
        QTest::addColumn<int>("code");
        const int invalid = static_cast<int>(ErrorCode::InvalidArgument);
        QTest::newRow("traversal") << QStringLiteral("<home>/Game/../.ssh/id_rsa") << QString() << invalid;
        QTest::newRow("glob") << QStringLiteral("<home>/Game/*.sav") << QString() << invalid;
        QTest::newRow("binding with separator") << QStringLiteral("<home>/Game/<storeUserId>/a.sav")
                                                << QStringLiteral("../../x") << invalid;
        QTest::newRow("binding is dot-dot") << QStringLiteral("<home>/Game/<storeUserId>/a.sav") << QStringLiteral("..")
                                            << invalid;
        QTest::newRow("missing binding") << QStringLiteral("<home>/Game/<storeUserId>/a.sav") << QString() << invalid;
        QTest::newRow("sensitive") << QStringLiteral("<home>/Game/.ssh/key") << QString()
                                   << static_cast<int>(ErrorCode::PermissionDenied);
        QTest::newRow("unavailable root") << QStringLiteral("<macAppSupport>/Game/a.sav") << QString()
                                          << static_cast<int>(ErrorCode::NotFound);
    }
    void restoreTargetsRejectUnsafeInput()
    {
        QFETCH(QString, path);
        QFETCH(QString, binding);
        QFETCH(int, code);
        QTemporaryDir tmp;
        QHash<QString, QString> bindings;
        if (!binding.isEmpty())
            bindings.insert(QStringLiteral("storeUserId"), binding);
        const auto target = resolveRestoreTarget(path, bindings, contextFor(tmp.path()));
        QVERIFY(!target.ok());
        QCOMPARE(static_cast<int>(target.error().code), code);
    }

    void restoreTargetsRefuseLinks()
    {
        QTemporaryDir tmp;
        QTemporaryDir outside;
        const ResolveContext ctx = contextFor(tmp.path());
        QVERIFY(QDir().mkpath(tmp.path() + QStringLiteral("/winDocuments/Game")));
        const QString link = tmp.path() + QStringLiteral("/winDocuments/Game/saves");
#if defined(Q_OS_WIN)
        QProcess mklink;
        mklink.setStandardOutputFile(QProcess::nullDevice());
        mklink.start(QStringLiteral("cmd"), {QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"),
                                             QDir::toNativeSeparators(link), QDir::toNativeSeparators(outside.path())});
        mklink.waitForFinished();
        if (!QFileInfo(link).isJunction())
            QSKIP("cannot create junctions on this system");
#else
        if (!QFile::link(outside.path(), link))
            QSKIP("cannot create symbolic links on this system");
#endif
        const auto target = resolveRestoreTarget(QStringLiteral("<winDocuments>/Game/saves/slot.sav"), {}, ctx);
        QVERIFY(!target.ok());
        QCOMPARE(target.error().code, ErrorCode::PermissionDenied);
    }

    void latestNKeepsNewestByModificationTime()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        const QString base = tmp.path() + QStringLiteral("/base");
        const QDateTime now = QDateTime::currentDateTimeUtc();
        touch(base, QStringLiteral("saves/a.sav"), now.addDays(-4));
        touch(base, QStringLiteral("saves/b.sav"), now.addDays(-1));
        touch(base, QStringLiteral("saves/c.sav"), now.addDays(-3));

        Profile profile = makeProfile({rule(QStringLiteral("autosaves"), {QStringLiteral("<base>/saves/*.sav")})});
        profile.mechanism.type = QStringLiteral("latest-n");
        profile.mechanism.params = QJsonObject{{QStringLiteral("n"), 2}};
        QCOMPARE(templatePaths(PathResolver().resolve(profile, ctx)), (QStringList{
            QStringLiteral("<base>/saves/b.sav"), QStringLiteral("<base>/saves/c.sav")}));
    }

    void latestNKeepsNewestDirectoriesByName()
    {
        QTemporaryDir tmp;
        const ResolveContext ctx = contextFor(tmp.path());
        const QString base = tmp.path() + QStringLiteral("/base");
        touch(base, QStringLiteral("saves/autosave_1/data"));
        touch(base, QStringLiteral("saves/autosave_2/data"));
        touch(base, QStringLiteral("saves/autosave_3/data"));

        Profile profile = makeProfile({rule(QStringLiteral("autosaves"), {QStringLiteral("<base>/saves/autosave_*")})});
        profile.mechanism.type = QStringLiteral("latest-n");
        profile.mechanism.params = QJsonObject{{QStringLiteral("n"), 2}, {QStringLiteral("sortBy"), QStringLiteral("name")}};
        QCOMPARE(templatePaths(PathResolver().resolve(profile, ctx)), (QStringList{
            QStringLiteral("<base>/saves/autosave_2/data"), QStringLiteral("<base>/saves/autosave_3/data")}));
    }
};

QTEST_GUILESS_MAIN(TestPathResolver)
#include "tst_path_resolver.moc"
