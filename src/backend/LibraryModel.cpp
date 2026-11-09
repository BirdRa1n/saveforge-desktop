#include "LibraryModel.h"

#include "Formatting.h"

#include <QCoreApplication>

namespace saveforge::backend {

LibraryModel::LibraryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void LibraryModel::setGames(QList<services::LibraryGame> games)
{
    beginResetModel();
    m_games = std::move(games);
    endResetModel();
    emit countChanged();
}

void LibraryModel::updateGame(const services::LibraryGame &game)
{
    for (qsizetype row = 0; row < m_games.size(); ++row) {
        if (m_games.at(row).id() == game.id()) {
            m_games[row] = game;
            const QModelIndex changed = index(static_cast<int>(row));
            emit dataChanged(changed, changed);
            return;
        }
    }
}

const services::LibraryGame *LibraryModel::game(const QString &id) const
{
    for (const services::LibraryGame &game : m_games) {
        if (game.id() == id)
            return &game;
    }
    return nullptr;
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_games.size());
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_games.size())
        return {};
    const services::LibraryGame &game = m_games.at(index.row());
    switch (role) {
    case GameIdRole: return game.id();
    case Qt::DisplayRole:
    case NameRole: return game.target.profile.name;
    case StoreRole: return storeLabel(game);
    case StoreKeyRole: return storeKey(game);
    case StatusTextRole:
        return game.lastSnapshot ? QCoreApplication::translate("SaveForge", "Backup %1").arg(relativeTime(game.lastSnapshot->createdAt))
                                 : QCoreApplication::translate("SaveForge", "Sem backup");
    case StatusKindRole: return game.lastSnapshot ? 0 : 2;
    case CoverColorRole: return coverColor(game.id());
    case SnapshotCountRole: return static_cast<int>(game.snapshotCount);
    case LauncherCloudRole: return game.launcherCloud;
    default: return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {{GameIdRole, "gameId"},           {NameRole, "name"},           {StoreRole, "store"},
            {StoreKeyRole, "storeKey"},       {StatusTextRole, "statusText"}, {StatusKindRole, "statusKind"},
            {CoverColorRole, "coverColor"},   {SnapshotCountRole, "snapshotCount"},
            {LauncherCloudRole, "launcherCloud"}};
}

LibraryFilterModel::LibraryFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortCaseSensitivity(Qt::CaseInsensitive);
    setSortRole(LibraryModel::NameRole);
    sort(0);
    connect(this, &QAbstractItemModel::rowsInserted, this, &LibraryFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &LibraryFilterModel::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &LibraryFilterModel::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &LibraryFilterModel::countChanged);
}

void LibraryFilterModel::setSearch(const QString &search)
{
    if (search == m_search)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_search = search;
    endFilterChange();
#else
    m_search = search;
    invalidateRowsFilter();
#endif
    emit searchChanged();
    emit countChanged();
}

void LibraryFilterModel::setStore(const QString &store)
{
    if (store == m_store)
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_store = store;
    endFilterChange();
#else
    m_store = store;
    invalidateRowsFilter();
#endif
    emit storeChanged();
    emit countChanged();
}

bool LibraryFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!m_store.isEmpty() && index.data(LibraryModel::StoreKeyRole).toString() != m_store)
        return false;
    return m_search.isEmpty() || index.data(LibraryModel::NameRole).toString().contains(m_search, Qt::CaseInsensitive);
}

} // namespace saveforge::backend
