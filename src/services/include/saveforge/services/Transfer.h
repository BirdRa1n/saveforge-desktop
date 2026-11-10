#pragma once

#include "saveforge/archive/Gamesave.h"
#include "saveforge/core/Result.h"
#include "saveforge/vault/Vault.h"

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <optional>

namespace saveforge::services {

class LocalProfileStore;

// What an export carried, kept so a later export can be incremental.
struct ExportRecord {
    QUuid exportId;
    QDateTime createdAt;
    QStringList gameIds;
    QSet<QString> blobs;     // blobs the archive carries itself
    QString path;            // where it was written
};

// Exports written on this machine, newest last, stored as JSON in one folder.
class ExportLog {
public:
    explicit ExportLog(const QString &directory);
    [[nodiscard]] Result<void> add(const ExportRecord &record);
    [[nodiscard]] std::optional<ExportRecord> latest() const;

private:
    QString m_dir;
};

struct ExportGame {
    QString gameId;
    QString name;
    std::optional<QJsonObject> localProfile; // for games only this machine knows
};

struct ExportRequest {
    QString path;                             // the .gamesave to write
    QList<ExportGame> games;
    bool allHistory = false;                  // false: only each game's newest snapshot
    std::optional<ExportRecord> base;         // set: incremental, omitting the base's blobs
    QString appVersion;
    QString sourceOs;
    QMap<QString, QString> placeholders;      // informative; empty to leave the user name out
};

// Writes the selected games' snapshots to a .gamesave (gamesave/SPEC.md).
// Games without snapshots are skipped; NotFound when none has any.
[[nodiscard]] Result<ExportRecord> exportGames(const vault::Vault &vault, const ExportRequest &request);

struct ImportReport {
    qsizetype imported = 0;          // snapshots added to the vault
    qsizetype alreadyPresent = 0;    // snapshots the vault had (imports are idempotent)
    QStringList games;               // names of the games in the archive
    QStringList addedLocalProfiles;  // local profiles stored for games this machine did not know
};

// Adds a .gamesave's snapshots to the vault without restoring anything
// (gamesave/SPEC.md section 5). An incremental export is refused, naming its
// base export, when the vault lacks a blob the archive omits. Local profiles
// in the archive are stored in `localProfiles` (when given) for games whose id
// is not in `knownGameIds`.
[[nodiscard]] Result<ImportReport> importGamesave(vault::Vault &vault, const archive::GamesaveReader &reader,
                                                  LocalProfileStore *localProfiles = nullptr,
                                                  const QSet<QString> &knownGameIds = {});

} // namespace saveforge::services
