#include "TrayIcon.h"
#include "UiHost.h"

#include "AppController.h"
#include "savevault/core/Version.h"

#include <QApplication>
#include <QIcon>
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

    // The controller (library, vault, automatic backups) belongs to the app,
    // not to the QML engine, so the interface can be unloaded in the tray.
    savevault::backend::AppController controller(nullptr);
    savevault::backend::AppController::setSharedInstance(&controller);

    savevault::app::UiHost ui;
    // With a tray icon, closing the window may only hide it (Main.qml decides
    // from the user's setting), so quitting is explicit and the hidden
    // interface is unloaded to free its memory.
    std::unique_ptr<savevault::app::TrayIcon> tray;
    if (savevault::app::TrayIcon::isSupported()) {
        tray = std::make_unique<savevault::app::TrayIcon>(&controller, &ui);
        QGuiApplication::setQuitOnLastWindowClosed(false);
        ui.setUnloadWhenHidden(true);
    }
    if (!ui.show())
        return EXIT_FAILURE;

    return QApplication::exec();
}
