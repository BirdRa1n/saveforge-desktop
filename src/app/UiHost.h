#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <QRect>
#include <QTimer>

#include <memory>

class QQmlApplicationEngine;

namespace savevault::app {

// Creates the QML interface on demand and, while the app runs in the tray,
// unloads it again.
//
// The interface (QML engine, scene graph, GPU resources, decoded images) is
// most of the app's memory, yet nothing uses it while the window is hidden:
// the AppController, which owns the library and runs the automatic backups,
// lives outside the engine (AppController::setSharedInstance). So a few
// seconds after the window is hidden, the engine is destroyed; show() builds
// it again, restoring the window geometry and the selected game. The library
// is not scanned again.
class UiHost : public QObject {
    Q_OBJECT

public:
    explicit UiHost(QObject *parent = nullptr);
    ~UiHost() override;

    // Creates the interface when needed, then shows, raises and activates the
    // window. False when the QML could not be loaded.
    bool show();
    // The current window, or null while the interface is unloaded.
    [[nodiscard]] QQuickWindow *window() const { return m_window; }
    [[nodiscard]] bool isLoaded() const { return m_engine != nullptr; }
    // Unloads the interface once the window has been hidden for `delayMs`.
    // Off by default: without a tray there is no way back to a hidden window.
    void setUnloadWhenHidden(bool enabled, int delayMs = 5000);

signals:
    // The window was hidden (closed to the tray) or shown.
    void windowVisibleChanged(bool visible);

private:
    bool load();
    void unload();

    std::unique_ptr<QQmlApplicationEngine> m_engine;
    QPointer<QQuickWindow> m_window;
    QTimer m_unloadTimer;
    bool m_unloadWhenHidden = false;
    // State carried over to the next window.
    QRect m_geometry;
    bool m_maximized = false;
    QString m_selectedGameId;
};

} // namespace savevault::app
