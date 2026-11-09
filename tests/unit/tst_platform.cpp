#include "saveforge/platform/AppPaths.h"
#include "saveforge/platform/Os.h"
#include "saveforge/platform/Processes.h"
#include "saveforge/platform/UserFolders.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using namespace saveforge::platform;

class TestPlatform : public QObject {
    Q_OBJECT

private slots:
    void osNamesRoundTrip()
    {
        for (Os os : {Os::Windows, Os::Linux, Os::MacOS})
            QCOMPARE(osFromName(osName(os)), os);
        QVERIFY(!osFromName(u"freebsd").has_value());
        QVERIFY(!osFromName(u"Windows").has_value()); // names are case-sensitive
    }

    void folderMapNormalizesSeparators()
    {
        FolderMap map;
        // Native separators are converted; backslashes only count as separators on Windows.
        map.set(QStringLiteral("home"), QDir::toNativeSeparators(QStringLiteral("/home/player/")));
        QCOMPARE(map.value(QStringLiteral("home")), QStringLiteral("/home/player"));
        map.set(QStringLiteral("home"), QString());
        QVERIFY(!map.contains(QStringLiteral("home")));
    }

    void hostFoldersAreAbsolute()
    {
        const FolderMap folders = hostUserFolders();
        QVERIFY(folders.contains(QStringLiteral("home")));
        for (const QString &name : folders.placeholders())
            QVERIFY2(QDir::isAbsolutePath(folders.value(name)), qPrintable(name));

        switch (hostOs()) {
        case Os::Windows:
            for (const char *name : {"winAppData", "winLocalAppData", "winLocalAppDataLow", "winDocuments"})
                QVERIFY2(folders.contains(QLatin1String(name)), name);
            QVERIFY(!folders.contains(QStringLiteral("xdgConfig")));
            break;
        case Os::Linux:
            QVERIFY(folders.contains(QStringLiteral("xdgData")));
            QVERIFY(folders.contains(QStringLiteral("xdgConfig")));
            QVERIFY(!folders.contains(QStringLiteral("winAppData")));
            break;
        case Os::MacOS:
            QVERIFY(folders.contains(QStringLiteral("macAppSupport")));
            QVERIFY(!folders.contains(QStringLiteral("winAppData")));
            break;
        }
    }

    void xdgRelativeValuesAreIgnored()
    {
        if (hostOs() != Os::Linux)
            QSKIP("XDG variables only apply on Linux");
        qputenv("XDG_CONFIG_HOME", "relative/path");
        QCOMPARE(hostUserFolders().value(QStringLiteral("xdgConfig")), QDir::homePath() + QStringLiteral("/.config"));
        qputenv("XDG_CONFIG_HOME", "/tmp/xdg-config");
        QCOMPARE(hostUserFolders().value(QStringLiteral("xdgConfig")), QStringLiteral("/tmp/xdg-config"));
        qunsetenv("XDG_CONFIG_HOME");
    }

    void listsTheCurrentProcess()
    {
        const qint64 self = QCoreApplication::applicationPid();
        const QList<ProcessInfo> processes = runningProcesses();
        QVERIFY(processes.size() > 1);
        const auto it = std::find_if(processes.begin(), processes.end(), [self](const ProcessInfo &p) { return p.pid == self; });
        QVERIFY(it != processes.end());
        QVERIFY2(it->name.startsWith(QLatin1String("tst_platform")), qPrintable(it->name));
        QVERIFY(it->executablePath.endsWith(it->name, Qt::CaseInsensitive));
    }

    void appDataDirectoryIsSharedAndOverridable()
    {
        qunsetenv("SAVEFORGE_DATA_DIR");
        const QString dir = appDataDirectory();
        QVERIFY(QDir::isAbsolutePath(dir));
        QVERIFY(dir.endsWith(QLatin1String("/SaveForge")));
        QCoreApplication::setApplicationName(QStringLiteral("something-else"));
        QCOMPARE(appDataDirectory(), dir);
        qputenv("SAVEFORGE_DATA_DIR", QDir::toNativeSeparators(QStringLiteral("/sandbox/data/")).toUtf8());
        QCOMPARE(appDataDirectory(), QStringLiteral("/sandbox/data"));
        qunsetenv("SAVEFORGE_DATA_DIR");
    }

    void winePrefixMapsInsideDriveC()
    {
        QTemporaryDir prefix;
        QVERIFY(prefix.isValid());
        const QString user = prefix.path() + QStringLiteral("/drive_c/users/steamuser");

        FolderMap folders = winePrefixFolders(prefix.path(), QStringLiteral("steamuser"));
        QCOMPARE(folders.value(QStringLiteral("home")), user);
        QCOMPARE(folders.value(QStringLiteral("winAppData")), user + QStringLiteral("/AppData/Roaming"));
        QCOMPARE(folders.value(QStringLiteral("winLocalAppDataLow")), user + QStringLiteral("/AppData/LocalLow"));
        QCOMPARE(folders.value(QStringLiteral("winProgramData")), prefix.path() + QStringLiteral("/drive_c/ProgramData"));
        QCOMPARE(folders.value(QStringLiteral("winDocuments")), user + QStringLiteral("/Documents"));
        QVERIFY(!folders.contains(QStringLiteral("xdgConfig")));

        // Legacy prefixes only have "My Documents".
        QVERIFY(QDir().mkpath(user + QStringLiteral("/My Documents")));
        folders = winePrefixFolders(prefix.path(), QStringLiteral("steamuser"));
        QCOMPARE(folders.value(QStringLiteral("winDocuments")), user + QStringLiteral("/My Documents"));

        QVERIFY(winePrefixFolders(QString(), QStringLiteral("steamuser")).placeholders().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPlatform)
#include "tst_platform.moc"
