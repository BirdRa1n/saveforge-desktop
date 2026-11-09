#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTest>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(SaveForge_UiPlugin)

// Garante que a janela principal carrega sem nenhum aviso de QML
// (tipos ausentes, bindings quebrados, propriedades inexistentes).
class TestUiSmoke : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
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

        QObject *library = window->findChild<QObject *>(QStringLiteral("libraryPage"));
        QVERIFY(library);

        // Opcional: SAVEFORGE_SCREENSHOT_DIR=<dir> salva uma captura de cada página.
        const QString shotDir = qEnvironmentVariable("SAVEFORGE_SCREENSHOT_DIR");
        QVERIFY(QTest::qWaitForWindowExposed(window));

        // Navegar por todas as páginas não deve gerar avisos.
        for (int page = 0; page < 4; ++page) {
            sidebar->setProperty("currentIndex", page);
            QCoreApplication::processEvents();
            if (!shotDir.isEmpty())
                QVERIFY(window->grabWindow().save(shotDir + QStringLiteral("/page-%1.png").arg(page)));
        }

        // A grade de jogos (GameCard + StatusBadge) também deve renderizar sem avisos.
        sidebar->setProperty("currentIndex", 0);
        QVariantList games;
        const auto addGame = [&games](const QString &name, const QString &store, const QString &status,
                                      int kind, const QString &color) {
            games.append(QVariantMap{{QStringLiteral("name"), name},
                                     {QStringLiteral("store"), store},
                                     {QStringLiteral("statusText"), status},
                                     {QStringLiteral("statusKind"), kind},
                                     {QStringLiteral("color"), color}});
        };
        // statusKind: 0 = Success, 1 = Warning, 2 = Neutral (enum StatusBadge.Kind)
        addGame(QStringLiteral("Hollow Knight"), QStringLiteral("Steam"), QStringLiteral("Backup há 2 h"), 0, QStringLiteral("#3B4A6B"));
        addGame(QStringLiteral("Stardew Valley"), QStringLiteral("GOG"), QStringLiteral("Conflito: Steam Cloud"), 1, QStringLiteral("#4F6B3B"));
        addGame(QStringLiteral("Hades"), QStringLiteral("Epic"), QStringLiteral("Sem backup"), 2, QStringLiteral("#7A2E22"));
        library->setProperty("games", games);
        QCoreApplication::processEvents();
        if (!shotDir.isEmpty())
            QVERIFY(window->grabWindow().save(shotDir + QStringLiteral("/library-with-games.png")));
    }
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
