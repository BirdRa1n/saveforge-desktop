#pragma once

#include "saveforge/services/GameArt.h"
#include "saveforge/services/Library.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

namespace saveforge::backend {

// Games of the library, one row per game (see services::scanLibrary).
class LibraryModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by AppController.library")
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        GameIdRole = Qt::UserRole + 1,
        NameRole,
        StoreRole,          // display label, e.g. "Steam"
        StoreKeyRole,       // filter key: steam, epic, gog, other
        StatusTextRole,     // "Backup há 2 h", "Sem backup", ...
        StatusKindRole,     // 0 success, 1 warning, 2 neutral (StatusBadge.Kind)
        CoverColorRole,
        SnapshotCountRole,
        LauncherCloudRole,
        CoverRole,          // file URL of the landscape cover, or of the icon; empty URL when none
        IconOnlyRole,       // true when CoverRole is a small icon rather than a cover
    };
    Q_ENUM(Role)

    explicit LibraryModel(QObject *parent = nullptr);

    void setGames(QList<services::LibraryGame> games);
    // Replaces one game (e.g. after a backup); no-op if the id is unknown.
    void updateGame(const services::LibraryGame &game);
    // Art of the games, by game id. Kept across setGames() until replaced.
    void setArt(QHash<QString, services::GameArt> art);
    [[nodiscard]] services::GameArt art(const QString &id) const { return m_art.value(id); }

    [[nodiscard]] const services::LibraryGame *game(const QString &id) const;
    [[nodiscard]] const QList<services::LibraryGame> &games() const { return m_games; }

    [[nodiscard]] int count() const { return static_cast<int>(m_games.size()); }
    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    QList<services::LibraryGame> m_games;
    QHash<QString, services::GameArt> m_art;
};

// Search and store filter on top of LibraryModel, for the library grid.
class LibraryFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(QString store READ store WRITE setStore NOTIFY storeChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit LibraryFilterModel(QObject *parent = nullptr);

    [[nodiscard]] QString search() const { return m_search; }
    void setSearch(const QString &search);
    // Empty for all stores, otherwise steam, epic, gog or other.
    [[nodiscard]] QString store() const { return m_store; }
    void setStore(const QString &store);
    [[nodiscard]] int count() const { return rowCount(); }

signals:
    void searchChanged();
    void storeChanged();
    void countChanged();

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_search;
    QString m_store;
};

} // namespace saveforge::backend
