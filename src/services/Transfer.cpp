#include "saveforge/services/Transfer.h"

#include "saveforge/core/Uuid.h"
#include "saveforge/services/LocalProfiles.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>

#include <algorithm>

Q_LOGGING_CATEGORY(lcTransfer, "saveforge.transfer")

namespace saveforge::services {

namespace {

QJsonObject toJsonObject(const ExportRecord &record)
{
    QStringList blobs(record.blobs.cbegin(), record.blobs.cend());
    blobs.sort();
    return {{QStringLiteral("exportId"), uuidToString(record.exportId)},
            {QStringLiteral("createdAt"), record.createdAt.toUTC().toString(Qt::ISODateWithMs)},
            {QStringLiteral("games"), QJsonArray::fromStringList(record.gameIds)},
            {QStringLiteral("blobs"), QJsonArray::fromStringList(blobs)},
            {QStringLiteral("path"), record.path}};
}

std::optional<ExportRecord> fromJson(const QByteArray &bytes)
{
    const QJsonObject o = QJsonDocument::fromJson(bytes).object();
    ExportRecord record;
    record.exportId = QUuid::fromString(o.value(QLatin1String("exportId")).toString());
    record.createdAt = QDateTime::fromString(o.value(QLatin1String("createdAt")).toString(), Qt::ISODateWithMs);
    if (record.exportId.isNull() || !record.createdAt.isValid())
        return std::nullopt;
    for (const QJsonValue &v : o.value(QLatin1String("games")).toArray())
        record.gameIds.append(v.toString());
    for (const QJsonValue &v : o.value(QLatin1String("blobs")).toArray())
        record.blobs.insert(v.toString());
    record.path = o.value(QLatin1String("path")).toString();
    return record;
}

} // namespace

ExportLog::ExportLog(const QString &directory)
    : m_dir(QDir::cleanPath(directory))
{
}

Result<void> ExportLog::add(const ExportRecord &record)
{
    if (!QDir().mkpath(m_dir))
        return Error{ErrorCode::Io, QStringLiteral("cannot create the export log"), m_dir};
    QFile file(m_dir + u'/' + uuidToString(record.exportId) + QStringLiteral(".json"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return Error{ErrorCode::Io, file.errorString(), file.fileName()};
    file.write(QJsonDocument(toJsonObject(record)).toJson());
    return {};
}

std::optional<ExportRecord> ExportLog::latest() const
{
    std::optional<ExportRecord> newest;
    for (const QString &name : QDir(m_dir).entryList({QStringLiteral("*.json")}, QDir::Files)) {
        QFile file(m_dir + u'/' + name);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const auto record = fromJson(file.readAll());
        if (record && (!newest || record->createdAt > newest->createdAt))
            newest = record;
    }
    return newest;
}

Result<ExportRecord> exportGames(const vault::Vault &vault, const ExportRequest &request)
{
    archive::GamesaveManifest manifest;
    manifest.exportId = createUuidV7();
    manifest.createdAt = QDateTime::currentDateTimeUtc();
    manifest.appVersion = request.appVersion;
    manifest.sourceOs = request.sourceOs;
    manifest.placeholders = request.placeholders;
    if (request.base)
        manifest.baseExportId = request.base->exportId;

    ExportRecord record;
    record.exportId = manifest.exportId;
    record.createdAt = manifest.createdAt;
    record.path = request.path;
    for (const ExportGame &game : request.games) {
        QList<vault::Snapshot> snapshots = vault.snapshots(game.gameId);
        if (snapshots.isEmpty())
            continue;
        if (!request.allHistory)
            snapshots = {snapshots.last()};
        manifest.games.append({game.gameId, game.name, game.localProfile, snapshots});
        record.gameIds.append(game.gameId);
    }
    if (manifest.games.isEmpty())
        return Error{ErrorCode::NotFound, QStringLiteral("none of the chosen games has a backup to export"), {}};

    QSet<QString> omit;
    if (request.base)
        omit = manifest.referencedBlobs() & request.base->blobs;
    record.blobs = manifest.referencedBlobs() - omit;

    const auto readBlob = [&vault](const QString &sha256) -> Result<QByteArray> {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        if (!buffer.open(QIODevice::WriteOnly))
            return Error{ErrorCode::Io, QStringLiteral("cannot read a backup file"), sha256};
        if (auto read = vault.blobs().read(sha256, buffer); !read.ok())
            return read.error();
        return bytes;
    };
    if (auto written = archive::writeGamesave(request.path, manifest, readBlob, omit); !written.ok())
        return written.error();
    qCInfo(lcTransfer) << "exported" << record.gameIds.size() << "game(s) to" << request.path
                       << (request.base ? "(incremental)" : "(full)");
    return record;
}

Result<ImportReport> importGamesave(vault::Vault &vault, const archive::GamesaveReader &reader,
                                    LocalProfileStore *localProfiles, const QSet<QString> &knownGameIds)
{
    const archive::GamesaveManifest &manifest = reader.manifest();
    // Incremental exports need their base: every omitted blob must already be here.
    for (const QString &missing : reader.missingBlobs()) {
        if (!vault.blobs().contains(missing))
            return Error{ErrorCode::NotFound,
                         QStringLiteral("this incremental export needs export %1 to be imported first")
                             .arg(manifest.baseExportId ? uuidToString(*manifest.baseExportId) : QString()),
                         missing};
    }

    ImportReport report;
    const auto source = [&reader](const QString &sha256) { return reader.blob(sha256); };
    for (const archive::GamesaveGame &game : manifest.games) {
        report.games.append(game.name);
        if (game.localProfile && localProfiles && !knownGameIds.contains(game.gameId)) {
            auto stored = localProfiles->importProfileJson(QJsonDocument(*game.localProfile).toJson());
            if (!stored.ok())
                return Error{stored.error().code,
                             QStringLiteral("cannot store the profile of %1: ").arg(game.name) + stored.error().message,
                             stored.error().context};
            report.addedLocalProfiles.append(game.name);
        }
        for (const vault::Snapshot &snapshot : game.snapshots) {
            auto added = vault.importSnapshot(snapshot, source);
            if (!added.ok())
                return Error{added.error().code,
                             QStringLiteral("cannot import a backup of %1: ").arg(game.name) + added.error().message,
                             added.error().context};
            if (added.value())
                ++report.imported;
            else
                ++report.alreadyPresent;
        }
    }
    qCInfo(lcTransfer) << "imported" << report.imported << "snapshot(s);" << report.alreadyPresent << "already present";
    return report;
}

} // namespace saveforge::services
