#include "UiHost.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

namespace savevault::app {

UiHost::UiHost(QObject *parent)
    : QObject(parent)
{
    m_unloadTimer.setSingleShot(true);
    connect(&m_unloadTimer, &QTimer::timeout, this, &UiHost::unload);
}

UiHost::~UiHost() = default;

void UiHost::setUnloadWhenHidden(bool enabled, int delayMs)
{
    m_unloadWhenHidden = enabled;
    m_unloadTimer.setInterval(delayMs);
    if (!enabled)
        m_unloadTimer.stop();
}

bool UiHost::load()
{
    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_engine->loadFromModule("SaveVault.Ui", "Main");
    if (m_engine->rootObjects().isEmpty()) {
        m_engine.reset();
        return false;
    }
    m_window = qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst());
    if (!m_window) {
        m_engine.reset();
        return false;
    }
    if (m_geometry.isValid())
        m_window->setGeometry(m_geometry);
    if (!m_selectedGameId.isEmpty())
        m_window->setProperty("selectedGameId", m_selectedGameId);

    connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
        if (QCoreApplication::closingDown())
            return;
        if (visible) {
            m_unloadTimer.stop();
        } else if (m_unloadWhenHidden) {
            m_unloadTimer.start();
        }
        emit windowVisibleChanged(visible);
    });
    return true;
}

void UiHost::unload()
{
    if (!m_engine || (m_window && m_window->isVisible()))
        return;
    if (m_window) {
        m_maximized = m_window->windowStates().testFlag(Qt::WindowMaximized);
        if (!m_maximized)
            m_geometry = m_window->geometry();
        m_selectedGameId = m_window->property("selectedGameId").toString();
    }
    m_window = nullptr;
    // Destroys the window, its scene graph and every QML object; the shared
    // AppController is in C++ ownership and stays.
    m_engine.reset();
}

bool UiHost::show()
{
    m_unloadTimer.stop();
    if (!m_engine && !load())
        return false;
    if (m_maximized && !m_window->isVisible())
        m_window->showMaximized();
    else
        m_window->show();
    if (m_window->windowStates().testFlag(Qt::WindowMinimized))
        m_window->setWindowStates(m_window->windowStates() & ~Qt::WindowMinimized);
    m_window->raise();
    m_window->requestActivate();
    return true;
}

} // namespace savevault::app
