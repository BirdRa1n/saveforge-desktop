#include "TrayIcon.h"

#include "AppController.h"

#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QQuickWindow>

namespace saveforge::app {

TrayIcon::TrayIcon(backend::AppController *controller, QQuickWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_menu(std::make_unique<QMenu>())
{
    m_icon.setIcon(QIcon(QStringLiteral(":/saveforge/icons/saveforge.png")));
    m_icon.setToolTip(QStringLiteral("SaveForge"));

    m_menu->addAction(tr("Abrir o SaveForge"), this, &TrayIcon::showWindow);
    m_menu->addAction(tr("Fazer backup de tudo"), controller, &backend::AppController::backupAll);
    m_menu->addSeparator();
    m_menu->addAction(tr("Sair"), qApp, &QCoreApplication::quit);
    m_icon.setContextMenu(m_menu.get());

    connect(&m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showWindow();
    });
    connect(&m_icon, &QSystemTrayIcon::messageClicked, this, &TrayIcon::showWindow);

    connect(controller, &backend::AppController::automaticBackupDone, this, [this](const QString &text) {
        if (m_window && !m_window->isVisible())
            m_icon.showMessage(QStringLiteral("SaveForge"), text, QSystemTrayIcon::Information, 5000);
    });
    // Tell the user once per session where the app went.
    if (m_window) {
        connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
            if (!visible && !m_hiddenNoticeShown && qApp && !QCoreApplication::closingDown()) {
                m_hiddenNoticeShown = true;
                m_icon.showMessage(QStringLiteral("SaveForge"),
                                   tr("O SaveForge continua aberto na bandeja e faz os backups automáticos. "
                                      "Para fechar de vez, use Sair no menu do ícone."),
                                   QSystemTrayIcon::Information, 6000);
            }
        });
    }

    m_icon.show();
    controller->setTrayAvailable(true);
}

TrayIcon::~TrayIcon() = default;

bool TrayIcon::isSupported()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon::showWindow()
{
    if (!m_window)
        return;
    m_window->show();
    if (m_window->windowStates().testFlag(Qt::WindowMinimized))
        m_window->setWindowStates(m_window->windowStates() & ~Qt::WindowMinimized);
    m_window->raise();
    m_window->requestActivate();
}

} // namespace saveforge::app
