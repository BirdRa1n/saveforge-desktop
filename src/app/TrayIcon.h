#pragma once

#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>

#include <memory>

class QMenu;
class QQuickWindow;

namespace savevault::backend {
class AppController;
}

namespace savevault::app {

// System tray icon that keeps SaveVault reachable while its window is hidden,
// so automatic backups keep running. Offers opening the window, backing up
// every game and quitting, and shows a notification when an automatic backup
// finishes while the window is hidden.
//
// Closing the window hides it instead of quitting when the user allows the app
// to run in the background (Main.qml asks AppController.trayAvailable and
// runInBackground).
class TrayIcon : public QObject {
    Q_OBJECT

public:
    TrayIcon(backend::AppController *controller, QQuickWindow *window, QObject *parent = nullptr);
    ~TrayIcon() override;

    // False when the desktop has no system tray.
    [[nodiscard]] static bool isSupported();

private:
    void showWindow();

    QPointer<QQuickWindow> m_window;
    QSystemTrayIcon m_icon;
    std::unique_ptr<QMenu> m_menu;
    bool m_hiddenNoticeShown = false;
};

} // namespace savevault::app
