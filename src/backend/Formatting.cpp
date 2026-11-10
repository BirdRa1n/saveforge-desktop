#include "Formatting.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QLocale>

namespace saveforge::backend {

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("SaveForge", text);
}
} // namespace

QString countText(qint64 n, const QString &singular, const QString &plural)
{
    return QLocale(QLocale::Portuguese, QLocale::Brazil).toString(n) + u' ' + (n == 1 ? singular : plural);
}

QString relativeTime(const QDateTime &when, const QDateTime &now)
{
    if (!when.isValid())
        return {};
    const qint64 seconds = when.secsTo(now);
    if (seconds < 60)
        return tr("agora");
    if (seconds < 3600)
        return tr("há %1 min").arg(seconds / 60);
    if (seconds < 24 * 3600)
        return tr("há %1 h").arg(seconds / 3600);
    const qint64 days = when.toLocalTime().date().daysTo(now.toLocalTime().date());
    if (days <= 1)
        return tr("ontem");
    if (days < 30)
        return tr("há %1 dias").arg(days);
    return when.toLocalTime().toString(QStringLiteral("dd/MM/yyyy"));
}

QString storeKey(const services::LibraryGame &game)
{
    const QString store = game.install ? game.install->store : QString();
    if (store == u"steam" || store == u"epic" || store == u"gog")
        return store;
    return QStringLiteral("other");
}

QString storeLabel(const services::LibraryGame &game)
{
    if (!game.install)
        return game.saveFilesFound > 0 ? tr("Saves encontrados") : tr("Somente backups");
    QString label;
    const QString &store = game.install->store;
    if (store == u"steam")
        label = QStringLiteral("Steam");
    else if (store == u"epic")
        label = QStringLiteral("Epic Games");
    else if (store == u"gog")
        label = QStringLiteral("GOG");
    else
        label = store;
    if (game.install->launcher == u"heroic")
        label += tr(" via Heroic");
    else if (game.install->launcher == u"lutris")
        label = label.isEmpty() ? QStringLiteral("Lutris") : label + tr(" via Lutris");
    return label;
}

QString reasonLabel(detection::MatchReason reason, bool hasInstall)
{
    switch (reason) {
    case detection::MatchReason::StoreId: return tr("Identificado pela loja");
    case detection::MatchReason::Executable: return tr("Identificado pelo executável");
    case detection::MatchReason::InstallDirName: return tr("Identificado pela pasta de instalação");
    case detection::MatchReason::SavesFound:
        return hasInstall ? tr("Identificado pelos saves") : tr("Encontrado pelos saves (jogo não detectado)");
    }
    return {};
}

QString snapshotKindLabel(vault::SnapshotKind kind)
{
    switch (kind) {
    case vault::SnapshotKind::Manual: return tr("Manual");
    case vault::SnapshotKind::Auto: return tr("Automático");
    case vault::SnapshotKind::PreRestore: return tr("Antes de restaurar");
    case vault::SnapshotKind::Import: return tr("Importado");
    }
    return {};
}

QColor coverColor(const QString &gameId)
{
    static const QList<QColor> palette{QColor(0x3B4A6B), QColor(0x7A3B5C), QColor(0x4F6B3B), QColor(0x6B5A3B),
                                       QColor(0x7A2E22), QColor(0x3B5F6B), QColor(0x2F5A45), QColor(0x5A3B6B)};
    const QByteArray digest = QCryptographicHash::hash(gameId.toUtf8(), QCryptographicHash::Sha256);
    return palette.at(static_cast<quint8>(digest.at(0)) % palette.size());
}

} // namespace saveforge::backend
