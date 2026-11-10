#include "TrayIcon.h"

#include "AppController.h"
#include "savevault/core/Version.h"

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickStyle>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(SaveVault_UiPlugin)
Q_IMPORT_QML_PLUGIN(SaveVault_BackendPlugin)

int main(int argc, char *argv[])
{
    // QApplication (Qt Widgets) provides the system tray icon and its menu.
    QApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("SaveVault"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("savevault.org"));
    QGuiApplication::setApplicationName(QStringLiteral("SaveVault"));
    QGuiApplication::setApplicationVersion(QString::fromUtf8(savevault::kVersion.data(),
                                                             static_cast<qsizetype>(savevault::kVersion.size())));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/savevault/icons/savevault.png")));
    // Lets Linux desktops match the window to savevault.desktop.
    QGuiApplication::setDesktopFileName(QStringLiteral("savevault"));

    // "Basic" é o estilo totalmente customizável e idêntico nos três SOs;
    // o visual Console × Studio é aplicado pelos componentes do módulo SaveVault.Ui.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("SaveVault.Ui", "Main");
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    // With a tray icon, closing the window may only hide it (Main.qml decides
    // from the user's setting), so quitting is explicit.
    std::unique_ptr<savevault::app::TrayIcon> tray;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    auto *controller = engine.singletonInstance<savevault::backend::AppController *>("SaveVault.Backend", "AppController");
    if (window && controller && savevault::app::TrayIcon::isSupported()) {
        tray = std::make_unique<savevault::app::TrayIcon>(controller, window);
        QGuiApplication::setQuitOnLastWindowClosed(false);
    }

    return QApplication::exec();
}
