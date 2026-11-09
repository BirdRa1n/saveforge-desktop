#include "saveforge/core/Version.h"

#include <QCommandLineParser>
#include <QCoreApplication>

// saveforge-cli: interface sem GUI para scripts e testes ponta a ponta.
// Os comandos (scan, backup, restore, export, import) chegam com a Fase 1.
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SaveForge"));
    QCoreApplication::setApplicationName(QStringLiteral("saveforge-cli"));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(saveforge::kVersion.data(),
                                                              static_cast<qsizetype>(saveforge::kVersion.size())));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("SaveForge — backup de saves de jogos de PC"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    parser.showHelp(0);
}
