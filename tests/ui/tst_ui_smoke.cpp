#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(SaveForge_UiPlugin)
Q_IMPORT_QML_PLUGIN(SaveForge_BackendPlugin)

// Garante que a janela principal carrega sem nenhum aviso de QML
// (tipos ausentes, bindings quebrados, propriedades inexistentes).
class TestUiSmoke : public QObject {
    Q_OBJECT

    QTemporaryDir m_data;
    bool m_preparedData = false;

private slots:
    void initTestCase()
    {
        // Settings, vault and registry cache go to a throwaway directory, unless a
        // screenshot run points SAVEFORGE_DATA_DIR at prepared data on purpose.
        QVERIFY(m_data.isValid());
        m_preparedData = !qEnvironmentVariableIsEmpty("SAVEFORGE_SCREENSHOT_DIR") && !qEnvironmentVariableIsEmpty("SAVEFORGE_DATA_DIR");
        if (!m_preparedData) {
            qputenv("SAVEFORGE_DATA_DIR", m_data.path().toUtf8());
            // The test must not download profiles from the network.
            QFile settings(m_data.path() + QStringLiteral("/settings.ini"));
            QVERIFY(settings.open(QIODevice::WriteOnly));
            settings.write("[registry]\nautoUpdate=false\n");
        }
        QQuickStyle::setStyle(QStringLiteral("Basic"));
        // Qualquer aviso vindo de um arquivo QML (tipos ausentes, bindings quebrados) falha o teste.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("\\.qml|QQmlApplicationEngine")));
    }

    void mainWindowLoads()
    {
        QQmlApplicationEngine engine;
        engine.loadFromModule("SaveForge.Ui", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QCOMPARE(window->title(), QStringLiteral("SaveForge"));

        QObject *sidebar = window->findChild<QObject *>(QStringLiteral("sidebar"));
        QVERIFY(sidebar);
        QCOMPARE(sidebar->property("currentIndex").toInt(), 0);

        // The bundled fonts are registered (Theme's FontLoaders), with every weight.
        const QStringList families = QFontDatabase::families();
        QVERIFY(families.contains(QStringLiteral("Inter")));
        QVERIFY(families.contains(QStringLiteral("Space Grotesk")));
        QVERIFY(QFontDatabase::isSmoothlyScalable(QStringLiteral("Inter"), QStringLiteral("SemiBold")));

        QObject *library = window->findChild<QObject *>(QStringLiteral("libraryPage"));
        QVERIFY(library);

        // Opcional: SAVEFORGE_SCREENSHOT_DIR=<dir> salva uma captura de cada página.
        const QString shotDir = qEnvironmentVariable("SAVEFORGE_SCREENSHOT_DIR");
        QVERIFY(QTest::qWaitForWindowExposed(window));

        // Navegar por todas as páginas não deve gerar avisos.
        for (int page = 0; page < 4; ++page) { // biblioteca, atividade, importar/exportar, configurações
            sidebar->setProperty("currentIndex", page);
            QCoreApplication::processEvents();
            if (!shotDir.isEmpty())
                QVERIFY(window->grabWindow().save(shotDir + QStringLiteral("/page-%1.png").arg(page)));
        }

        // The first scan runs in the background; wait for it to finish.
        auto *controller = engine.singletonInstance<QObject *>("SaveForge.Backend", "AppController");
        QVERIFY(controller);
        QTRY_VERIFY_WITH_TIMEOUT(!controller->property("busy").toBool(), 60000);
        QVERIFY(controller->property("vaultReady").toBool());
        if (!m_preparedData) {
            // The data directory is empty: no profiles, and a fresh vault was created there.
            QCOMPARE(controller->property("profileCount").toInt(), 0);
            QVERIFY(controller->property("vaultPath").toString().startsWith(QDir::fromNativeSeparators(m_data.path())));
        }
        if (!shotDir.isEmpty()) {
            // Screenshot runs: the library after the scan, and one game page
            // (SAVEFORGE_SCREENSHOT_GAME, backed up first so it has history).
            sidebar->setProperty("currentIndex", 0);
            QTest::qWait(200);
            QVERIFY(window->grabWindow().save(shotDir + QStringLiteral("/library-scanned.png")));
            const QString game = qEnvironmentVariable("SAVEFORGE_SCREENSHOT_GAME");
            if (!game.isEmpty()) {
                QMetaObject::invokeMethod(controller, "backupGame", Q_ARG(QString, game));
                QTRY_VERIFY_WITH_TIMEOUT(!controller->property("busy").toBool(), 60000);
                window->setProperty("selectedGameId", game);
                QTest::qWait(200);
                QVERIFY(window->grabWindow().save(shotDir + QStringLiteral("/game-%1.png").arg(game)));
                // The restore dialog for the newest snapshot (plan only; nothing is restored).
                QVariantList history;
                QMetaObject::invokeMethod(controller, "snapshots", Q_RETURN_ARG(QVariantList, history), Q_ARG(QString, game));
                auto *dialog = window->findChild<QObject *>(QStringLiteral("restoreDialog"));
                if (dialog && !history.isEmpty()) {
                    QMetaObject::invokeMethod(dialog, "openFor", Q_ARG(QVariant, history.first()));
                    QTest::qWait(300);
                    QVERIFY(window->grabWindow().save(shotDir + QStringLiteral("/restore-%1.png").arg(game)));
                    QMetaObject::invokeMethod(dialog, "close");
                }
                window->setProperty("selectedGameId", QString());
            }
        }

        // The add-game dialog opens without warnings.
        auto *addDialog = window->findChild<QObject *>(QStringLiteral("addGameDialog"));
        QVERIFY(addDialog);
        QMetaObject::invokeMethod(addDialog, "openEmpty");
        QCoreApplication::processEvents();
        if (!shotDir.isEmpty())
            QVERIFY(window->grabWindow().save(shotDir + QStringLiteral("/add-game.png")));
        QMetaObject::invokeMethod(addDialog, "close");

#if defined(Q_OS_WIN)
        // Adding a game by hand: the temporary data directory lies in the user's
        // local AppData, so it can hold the save folder. Removing it again keeps backups.
        if (!m_preparedData) {
            const QString saves = m_data.path() + QStringLiteral("/Manual Game/Saves");
            QVERIFY(QDir().mkpath(saves));
            QFile save(saves + QStringLiteral("/slot.sav"));
            QVERIFY(save.open(QIODevice::WriteOnly));
            save.write("progress");
            save.close();
            QString error;
            QMetaObject::invokeMethod(controller, "addManualGame", Q_RETURN_ARG(QString, error),
                                      Q_ARG(QString, QStringLiteral("Manual Game")),
                                      Q_ARG(QVariantList, QVariantList{QUrl::fromLocalFile(saves)}));
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QTRY_VERIFY_WITH_TIMEOUT(!controller->property("busy").toBool(), 60000);
            QVariantMap info;
            QMetaObject::invokeMethod(controller, "gameInfo", Q_RETURN_ARG(QVariantMap, info),
                                      Q_ARG(QString, QStringLiteral("local-manual-game")));
            QCOMPARE(info.value(QStringLiteral("local")).toBool(), true);
            QMetaObject::invokeMethod(controller, "removeLocalGame", Q_ARG(QString, QStringLiteral("local-manual-game")));
            QTRY_VERIFY_WITH_TIMEOUT(!controller->property("busy").toBool(), 60000);
            QMetaObject::invokeMethod(controller, "gameInfo", Q_RETURN_ARG(QVariantMap, info),
                                      Q_ARG(QString, QStringLiteral("local-manual-game")));
            QVERIFY(info.isEmpty());
        }
#endif

        // The game page must render without warnings even for an unknown game.
        sidebar->setProperty("currentIndex", 0);
        window->setProperty("selectedGameId", QStringLiteral("unknown-game"));
        QCoreApplication::processEvents();
        QVERIFY(window->findChild<QObject *>(QStringLiteral("gameDetailPage")));
        if (!shotDir.isEmpty())
            QVERIFY(window->grabWindow().save(shotDir + QStringLiteral("/game-detail.png")));
        window->setProperty("selectedGameId", QString());
    }
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
