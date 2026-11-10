#pragma once

#include "saveforge/core/Result.h"
#include "saveforge/profiles/Profile.h"
#include "saveforge/vault/Snapshot.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QUuid>

#include <functional>
#include <memory>
#include <optional>

namespace saveforge::archive {

// `.gamesave` export files (saveforge-specs/gamesave/SPEC.md): a ZIP archive
// with a manifest that embeds vault snapshot documents, plus the original
// bytes of every file the snapshots reference.

inline constexpr const char *kGamesaveMediaType = "application/vnd.saveforge.gamesave+zip";
inline constexpr const char *kGamesaveExtension = ".gamesave";

struct GamesaveGame {
    QString gameId;
    QString name;
    // The user-created profile, for games other machines cannot know (local
    // profiles). Kept as the original JSON so it round-trips unchanged.
    std::optional<QJsonObject> localProfile;
    QList<vault::Snapshot> snapshots;
};

struct GamesaveManifest {
    QUuid exportId;
    QDateTime createdAt;                     // UTC
    QString app = QStringLiteral("SaveForge Desktop");
    QString appVersion;
    std::optional<QUuid> baseExportId;       // set: incremental export (blobs of the base are omitted)
    QString sourceOs;                        // windows, linux or macos
    QMap<QString, QString> placeholders;     // "<home>" -> path; informative only, may be empty
    QList<GamesaveGame> games;

    [[nodiscard]] bool incremental() const { return baseExportId.has_value(); }
    // Every blob hash the snapshots reference.
    [[nodiscard]] QSet<QString> referencedBlobs() const;
};

[[nodiscard]] QByteArray toJson(const GamesaveManifest &manifest);

// Parses manifest.json and applies the semantic rules of the spec: the format
// marker and version (Unsupported for a newer formatVersion), the mode and its
// base, snapshots that belong to their game, unique snapshot ids, path safety
// of every file (through the snapshot parser) and a valid localProfile for
// "local:" games. Errors carry the JSON pointer of the offending value.
[[nodiscard]] Result<GamesaveManifest> parseManifest(const QByteArray &json);

struct GamesaveLimits {
    qint64 maxTotalBytes = 16LL * 1024 * 1024 * 1024; // uncompressed, all entries
    qint64 maxCompressionRatio = 1000;
};

// Supplies the original bytes of a blob by SHA-256.
using BlobProvider = std::function<Result<QByteArray>(const QString &sha256)>;

// Writes a .gamesave to `path`: mimetype first and stored, the manifest and
// its SHA-256, and one blobs/<sha256> entry per referenced blob except those
// in `omitBlobs` (incremental exports). Entries are deflated unless that does
// not help or would exceed the compression ratio readers accept. The archive
// is written next to `path`, reopened and verified, then renamed into place.
[[nodiscard]] Result<void> writeGamesave(const QString &path, const GamesaveManifest &manifest,
                                         const BlobProvider &blobs, const QSet<QString> &omitBlobs = {});

// A .gamesave that passed every check of the spec (section 3): container,
// entry names, no encrypted or special entries, resource limits, manifest hash
// and schema, and the hash of every blob. Blobs are read on demand.
class GamesaveReader {
public:
    [[nodiscard]] static Result<GamesaveReader> open(const QString &path, const GamesaveLimits &limits = {});

    GamesaveReader(GamesaveReader &&) noexcept;
    GamesaveReader &operator=(GamesaveReader &&) noexcept;
    ~GamesaveReader();

    [[nodiscard]] const GamesaveManifest &manifest() const;
    [[nodiscard]] bool hasBlob(const QString &sha256) const;
    [[nodiscard]] Result<QByteArray> blob(const QString &sha256) const;
    // Referenced blobs the archive does not carry (incremental exports).
    [[nodiscard]] QSet<QString> missingBlobs() const;

private:
    struct Impl;
    explicit GamesaveReader(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};

} // namespace saveforge::archive
