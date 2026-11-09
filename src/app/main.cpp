#include "saveforge/core/Version.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(SaveForge_UiPlugin)
Q_IMPORT_QML_PLUGIN(SaveForge_BackendPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
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

    return QGuiApplication::exec();
}
