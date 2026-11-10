#include "TrayIcon.h"

#include "AppController.h"
#include "saveforge/core/Version.h"

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(SaveForge_UiPlugin)
Q_IMPORT_QML_PLUGIN(SaveForge_BackendPlugin)

int main(int argc, char *argv[])
{
    // QApplication (Qt Widgets) provides the system tray icon and its menu.
    QApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("SaveForge"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("saveforge.org"));
    QGuiApplication::setApplicationName(QStringLiteral("SaveForge"));
    QGuiApplication::setApplicationVersion(QString::fromUtf8(saveforge::kVersion.data(),
                                                             static_cast<qsizetype>(saveforge::kVersion.size())));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/saveforge/icons/saveforge.png")));
    // Lets Linux desktops match the window to saveforge.desktop.
    QGuiApplication::setDesktopFileName(QStringLiteral("saveforge"));

    // "Basic" é o estilo totalmente customizável e idêntico nos três SOs;
    // o visual Forja é aplicado pelos componentes do módulo SaveForge.Ui.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("SaveForge.Ui", "Main");
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    // With a tray icon, closing the window may only hide it (Main.qml decides
    // from the user's setting), so quitting is explicit.
    std::unique_ptr<saveforge::app::TrayIcon> tray;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    auto *controller = engine.singletonInstance<saveforge::backend::AppController *>("SaveForge.Backend", "AppController");
    if (window && controller && saveforge::app::TrayIcon::isSupported()) {
        tray = std::make_unique<saveforge::app::TrayIcon>(controller, window);
        QGuiApplication::setQuitOnLastWindowClosed(false);
    }

    return QApplication::exec();
}
