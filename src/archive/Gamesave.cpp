#include "saveforge/archive/Gamesave.h"

#include "saveforge/core/JsonReader.h"
#include "saveforge/core/Uuid.h"
#include "saveforge/platform/FileSync.h"
#include "saveforge/profiles/ProfileParser.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <miniz.h>

#include <cstring>

namespace saveforge::archive {

namespace {

const QByteArray kMimetype("application/vnd.saveforge.gamesave+zip");
const QString kMimetypeEntry = QStringLiteral("mimetype");
const QString kManifestEntry = QStringLiteral("manifest.json");
const QString kManifestHashEntry = QStringLiteral("manifest.json.sha256");
const QString kReadmeEntry = QStringLiteral("README.txt");
const QString kBlobPrefix = QStringLiteral("blobs/");
const QStringList kOses{QStringLiteral("windows"), QStringLiteral("linux"), QStringLiteral("macos")};

const QRegularExpression &sha256Pattern()
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{64}$"));
    return re;
}

const QRegularExpression &uuidPattern()
{
    static const QRegularExpression re(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return re;
}

QString timestamp(const QDateTime &dt)
{
    return dt.toUTC().toString(Qt::ISODateWithMs);
}

bool readUuid(JsonReader &r, const QJsonValue &value, const QString &pointer, QUuid &out)
{
    QString text;
    if (!r.string(value, pointer, text, 36, 36, &uuidPattern()))
        return false;
    out = QUuid::fromString(text);
    return true;
}

QString sha256Hex(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

Error invalid(const QString &message, const QString &context = {})
{
    return Error{ErrorCode::Corrupted, message, context};
}

// --- QFile-backed I/O for miniz, so paths work the same on every OS ---------

size_t writeToFile(void *opaque, mz_uint64 offset, const void *data, size_t size)
{
    auto *file = static_cast<QFile *>(opaque);
    if (file->pos() != static_cast<qint64>(offset) && !file->seek(static_cast<qint64>(offset)))
        return 0;
    return static_cast<size_t>(file->write(static_cast<const char *>(data), static_cast<qint64>(size)));
}

size_t readFromFile(void *opaque, mz_uint64 offset, void *data, size_t size)
{
    auto *file = static_cast<QFile *>(opaque);
    if (!file->seek(static_cast<qint64>(offset)))
        return 0;
    const qint64 read = file->read(static_cast<char *>(data), static_cast<qint64>(size));
    return read < 0 ? 0 : static_cast<size_t>(read);
}

// Deflates unless that gains nothing or the ratio would exceed what readers accept.
mz_uint levelFor(const QByteArray &bytes, qint64 maxRatio)
{
    if (bytes.size() < 64)
        return MZ_NO_COMPRESSION;
    mz_ulong bound = mz_compressBound(static_cast<mz_ulong>(bytes.size()));
    QByteArray out(static_cast<qsizetype>(bound), Qt::Uninitialized);
    if (mz_compress2(reinterpret_cast<unsigned char *>(out.data()), &bound,
                     reinterpret_cast<const unsigned char *>(bytes.constData()), static_cast<mz_ulong>(bytes.size()),
                     MZ_BEST_SPEED) != MZ_OK)
        return MZ_NO_COMPRESSION;
    const qint64 compressed = static_cast<qint64>(bound);
    if (compressed >= bytes.size() * 9 / 10)
        return MZ_NO_COMPRESSION;
    // Keep a margin below the reader's limit.
    if (bytes.size() / qMax<qint64>(1, compressed) > maxRatio / 2)
        return MZ_NO_COMPRESSION;
    return MZ_BEST_SPEED;
}

bool safeEntryName(const QString &name)
{
    if (name.isEmpty() || name.startsWith(u'/') || name.contains(u'\\') || name.contains(u':'))
        return false;
    for (const QString &segment : name.split(u'/')) {
        if (segment.isEmpty() || segment == u".." || segment == u".")
            return false;
    }
    return true;
}

} // namespace

QSet<QString> GamesaveManifest::referencedBlobs() const
{
    QSet<QString> hashes;
    for (const GamesaveGame &game : games) {
        for (const vault::Snapshot &snapshot : game.snapshots) {
            for (const vault::SnapshotFile &file : snapshot.files)
                hashes.insert(file.sha256);
        }
    }
    return hashes;
}

QByteArray toJson(const GamesaveManifest &manifest)
{
    QJsonObject source{{QStringLiteral("os"), manifest.sourceOs}};
    if (!manifest.placeholders.isEmpty()) {
        QJsonObject placeholders;
        for (auto it = manifest.placeholders.cbegin(); it != manifest.placeholders.cend(); ++it)
            placeholders.insert(it.key(), it.value());
        source.insert(QStringLiteral("placeholders"), placeholders);
    }
    QJsonArray games;
    for (const GamesaveGame &game : manifest.games) {
        QJsonArray snapshots;
        for (const vault::Snapshot &snapshot : game.snapshots)
            snapshots.append(QJsonDocument::fromJson(vault::toJson(snapshot)).object());
        QJsonObject entry{{QStringLiteral("gameId"), game.gameId},
                          {QStringLiteral("name"), game.name},
                          {QStringLiteral("snapshots"), snapshots}};
        if (game.localProfile)
            entry.insert(QStringLiteral("localProfile"), *game.localProfile);
        games.append(entry);
    }
    QJsonObject o{{QStringLiteral("format"), QStringLiteral("saveforge.gamesave")},
                  {QStringLiteral("formatVersion"), 1},
                  {QStringLiteral("exportId"), uuidToString(manifest.exportId)},
                  {QStringLiteral("createdAt"), timestamp(manifest.createdAt)},
                  {QStringLiteral("createdBy"), QJsonObject{{QStringLiteral("app"), manifest.app},
                                                            {QStringLiteral("version"), manifest.appVersion}}},
                  {QStringLiteral("mode"), manifest.incremental() ? QStringLiteral("incremental") : QStringLiteral("full")},
                  {QStringLiteral("source"), source},
                  {QStringLiteral("games"), games}};
    if (manifest.baseExportId)
        o.insert(QStringLiteral("baseExportId"), uuidToString(*manifest.baseExportId));
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

Result<GamesaveManifest> parseManifest(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("manifest is not a JSON object"), parseError.errorString()};
    const QJsonObject o = doc.object();

    JsonReader r;
    GamesaveManifest m;
    r.keys(o, QString(),
           {"format", "formatVersion", "exportId", "createdAt", "createdBy", "mode", "baseExportId", "source", "games"},
           {"format", "formatVersion", "exportId", "createdAt", "createdBy", "mode", "source", "games"});
    if (r.failed())
        return r.error();

    QString format;
    r.enumeration(o.value(QLatin1String("format")), QStringLiteral("/format"), format, {QStringLiteral("saveforge.gamesave")});
    qint64 version = 0;
    if (r.integer(o.value(QLatin1String("formatVersion")), QStringLiteral("/formatVersion"), version, 1) && version != 1)
        return Error{ErrorCode::Unsupported,
                     QStringLiteral("this .gamesave uses format version %1; update SaveForge to open it").arg(version),
                     QStringLiteral("/formatVersion")};
    readUuid(r, o.value(QLatin1String("exportId")), QStringLiteral("/exportId"), m.exportId);
    QString created;
    if (r.string(o.value(QLatin1String("createdAt")), QStringLiteral("/createdAt"), created, 20, 40)) {
        m.createdAt = QDateTime::fromString(created, Qt::ISODateWithMs).toUTC();
        if (!m.createdAt.isValid())
            r.fail(QStringLiteral("/createdAt"), QStringLiteral("must be an RFC 3339 date-time"));
    }
    QJsonObject createdBy;
    if (r.object(o.value(QLatin1String("createdBy")), QStringLiteral("/createdBy"), createdBy)
        && r.keys(createdBy, QStringLiteral("/createdBy"), {"app", "version"}, {"app", "version"})) {
        r.string(createdBy.value(QLatin1String("app")), QStringLiteral("/createdBy/app"), m.app, 1, 100);
        r.string(createdBy.value(QLatin1String("version")), QStringLiteral("/createdBy/version"), m.appVersion, 5, 64);
    }
    QString mode;
    r.enumeration(o.value(QLatin1String("mode")), QStringLiteral("/mode"), mode,
                  {QStringLiteral("full"), QStringLiteral("incremental")});
    if (!r.failed()) {
        const bool hasBase = o.contains(QLatin1String("baseExportId"));
        if (mode == u"incremental" && !hasBase)
            r.fail(QStringLiteral("/baseExportId"), QStringLiteral("incremental exports name their base export"));
        else if (mode == u"full" && hasBase)
            r.fail(QStringLiteral("/baseExportId"), QStringLiteral("full exports have no base export"));
        else if (hasBase) {
            QUuid base;
            if (readUuid(r, o.value(QLatin1String("baseExportId")), QStringLiteral("/baseExportId"), base))
                m.baseExportId = base;
        }
    }
    QJsonObject source;
    if (r.object(o.value(QLatin1String("source")), QStringLiteral("/source"), source)
        && r.keys(source, QStringLiteral("/source"), {"os", "placeholders"}, {"os"})) {
        r.enumeration(source.value(QLatin1String("os")), QStringLiteral("/source/os"), m.sourceOs, kOses);
        if (source.contains(QLatin1String("placeholders"))) {
            QJsonObject placeholders;
            if (r.object(source.value(QLatin1String("placeholders")), QStringLiteral("/source/placeholders"), placeholders)) {
                static const QRegularExpression key(QStringLiteral("^<[A-Za-z]+>$"));
                for (auto it = placeholders.constBegin(); it != placeholders.constEnd() && !r.failed(); ++it) {
                    const QString pointer = QStringLiteral("/source/placeholders/") + it.key();
                    if (!key.match(it.key()).hasMatch()) {
                        r.fail(pointer, QStringLiteral("must be a root placeholder like <home>"));
                        break;
                    }
                    QString value;
                    if (r.string(it.value(), pointer, value, 0, 1024))
                        m.placeholders.insert(it.key(), value);
                }
            }
        }
    }
    if (r.failed())
        return r.error();

    QJsonArray games;
    if (!r.array(o.value(QLatin1String("games")), QStringLiteral("/games"), games, 1))
        return r.error();
    QSet<QUuid> snapshotIds;
    for (qsizetype g = 0; g < games.size(); ++g) {
        const QString gp = QStringLiteral("/games/%1").arg(g);
        QJsonObject entry;
        if (!r.object(games.at(g), gp, entry)
            || !r.keys(entry, gp, {"gameId", "name", "localProfile", "snapshots"}, {"gameId", "name", "snapshots"}))
            return r.error();
        GamesaveGame game;
        r.string(entry.value(QLatin1String("gameId")), gp + QStringLiteral("/gameId"), game.gameId, 1, 100);
        r.string(entry.value(QLatin1String("name")), gp + QStringLiteral("/name"), game.name, 1, 200);
        if (r.failed())
            return r.error();
        if (entry.contains(QLatin1String("localProfile"))) {
            QJsonObject profile;
            if (!r.object(entry.value(QLatin1String("localProfile")), gp + QStringLiteral("/localProfile"), profile))
                return r.error();
            auto parsed = profiles::parseProfile(profile);
            if (!parsed.ok())
                return Error{parsed.error().code, QStringLiteral("local profile is invalid: ") + parsed.error().message,
                             gp + QStringLiteral("/localProfile") + parsed.error().context};
            game.localProfile = profile;
        } else if (game.gameId.startsWith(QLatin1String("local:"))) {
            return Error{ErrorCode::InvalidArgument, QStringLiteral("local games must carry their profile"),
                         gp + QStringLiteral("/localProfile")};
        }
        QJsonArray snapshots;
        if (!r.array(entry.value(QLatin1String("snapshots")), gp + QStringLiteral("/snapshots"), snapshots, 1))
            return r.error();
        for (qsizetype s = 0; s < snapshots.size(); ++s) {
            const QString sp = gp + QStringLiteral("/snapshots/%1").arg(s);
            if (!snapshots.at(s).isObject())
                return Error{ErrorCode::InvalidArgument, QStringLiteral("must be an object"), sp};
            auto snapshot = vault::parseSnapshot(snapshots.at(s).toObject());
            if (!snapshot.ok())
                return Error{snapshot.error().code, snapshot.error().message, sp + snapshot.error().context};
            if (snapshot.value().gameId != game.gameId)
                return Error{ErrorCode::InvalidArgument, QStringLiteral("snapshot belongs to another game"),
                             sp + QStringLiteral("/gameId")};
            if (snapshotIds.contains(snapshot.value().snapshotId))
                return Error{ErrorCode::InvalidArgument, QStringLiteral("duplicate snapshot id"),
                             sp + QStringLiteral("/snapshotId")};
            snapshotIds.insert(snapshot.value().snapshotId);
            game.snapshots.append(std::move(snapshot).value());
        }
        m.games.append(std::move(game));
    }
    return m;
}

// --- Writing -----------------------------------------------------------------

Result<void> writeGamesave(const QString &path, const GamesaveManifest &manifest, const BlobProvider &blobs,
                           const QSet<QString> &omitBlobs)
{
    const QByteArray manifestJson = toJson(manifest);
    if (auto check = parseManifest(manifestJson); !check.ok())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("export manifest is invalid: ") + check.error().message,
                     check.error().context};

    const QString tmp = path + u'.' + uuidToString(createUuidV7()).right(12) + QStringLiteral(".tmp");
    QFile out(tmp);
    if (!out.open(QIODevice::ReadWrite | QIODevice::NewOnly))
        return Error{ErrorCode::Io, out.errorString(), tmp};
    const auto fail = [&](const Error &error) -> Result<void> {
        out.close();
        QFile::remove(tmp);
        return error;
    };

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    zip.m_pWrite = &writeToFile;
    zip.m_pIO_opaque = &out;
    if (!mz_zip_writer_init_v2(&zip, 0, 0))
        return fail(Error{ErrorCode::Io, QStringLiteral("cannot start the archive"), tmp});
    const auto add = [&](const QString &name, const QByteArray &bytes, mz_uint level) {
        return mz_zip_writer_add_mem(&zip, name.toUtf8().constData(), bytes.constData(),
                                     static_cast<size_t>(bytes.size()), level) == MZ_TRUE;
    };
    const GamesaveLimits limits;
    bool ok = add(kMimetypeEntry, kMimetype, MZ_NO_COMPRESSION) && add(kManifestEntry, manifestJson, MZ_BEST_SPEED)
           && add(kManifestHashEntry, sha256Hex(manifestJson).toLatin1() + '\n', MZ_NO_COMPRESSION);
    QStringList hashes(manifest.referencedBlobs().cbegin(), manifest.referencedBlobs().cend());
    hashes.sort();
    std::optional<Error> blobError;
    for (const QString &hash : std::as_const(hashes)) {
        if (!ok)
            break;
        if (omitBlobs.contains(hash))
            continue;
        auto bytes = blobs(hash);
        if (!bytes.ok()) {
            blobError = bytes.error();
            break;
        }
        if (sha256Hex(bytes.value()) != hash) {
            blobError = Error{ErrorCode::Corrupted, QStringLiteral("a backup file does not match its hash"), hash};
            break;
        }
        ok = add(kBlobPrefix + hash, bytes.value(), levelFor(bytes.value(), limits.maxCompressionRatio));
    }
    ok = ok && !blobError && mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);
    if (blobError)
        return fail(*blobError);
    if (!ok || !platform::syncFile(out))
        return fail(Error{ErrorCode::Io, QStringLiteral("cannot write the archive"), tmp});
    out.close();

    // Verify what was written before reporting success (spec section 2).
    if (auto check = GamesaveReader::open(tmp); !check.ok()) {
        QFile::remove(tmp);
        return Error{check.error().code, QStringLiteral("the written archive does not verify: ") + check.error().message,
                     check.error().context};
    }
    QFile::remove(path);
    if (!QFile::rename(tmp, path)) {
        QFile::remove(tmp);
        return Error{ErrorCode::Io, QStringLiteral("cannot move the archive into place"), path};
    }
    return {};
}

// --- Reading -----------------------------------------------------------------

struct GamesaveReader::Impl {
    QFile file;
    mz_zip_archive zip{};
    bool zipOpen = false;
    GamesaveManifest manifest;
    QHash<QString, mz_uint> blobIndex; // sha256 -> entry index

    ~Impl()
    {
        if (zipOpen)
            mz_zip_reader_end(&zip);
    }

    Result<QByteArray> extract(mz_uint index, qint64 size)
    {
        QByteArray bytes(static_cast<qsizetype>(size), Qt::Uninitialized);
        if (!mz_zip_reader_extract_to_mem(&zip, index, bytes.data(), static_cast<size_t>(bytes.size()), 0))
            return invalid(QStringLiteral("an archive entry is damaged"), QString::number(index));
        return bytes;
    }
};

GamesaveReader::GamesaveReader(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}
GamesaveReader::GamesaveReader(GamesaveReader &&) noexcept = default;
GamesaveReader &GamesaveReader::operator=(GamesaveReader &&) noexcept = default;
GamesaveReader::~GamesaveReader() = default;

const GamesaveManifest &GamesaveReader::manifest() const
{
    return m_impl->manifest;
}

bool GamesaveReader::hasBlob(const QString &sha256) const
{
    return m_impl->blobIndex.contains(sha256);
}

QSet<QString> GamesaveReader::missingBlobs() const
{
    QSet<QString> missing = m_impl->manifest.referencedBlobs();
    for (auto it = m_impl->blobIndex.cbegin(); it != m_impl->blobIndex.cend(); ++it)
        missing.remove(it.key());
    return missing;
}

Result<QByteArray> GamesaveReader::blob(const QString &sha256) const
{
    const auto it = m_impl->blobIndex.constFind(sha256);
    if (it == m_impl->blobIndex.cend())
        return Error{ErrorCode::NotFound, QStringLiteral("the archive does not carry this file"), sha256};
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&m_impl->zip, *it, &stat))
        return invalid(QStringLiteral("an archive entry is damaged"), sha256);
    auto bytes = m_impl->extract(*it, static_cast<qint64>(stat.m_uncomp_size));
    if (bytes.ok() && sha256Hex(bytes.value()) != sha256)
        return invalid(QStringLiteral("a file in the archive does not match its hash"), sha256);
    return bytes;
}

Result<GamesaveReader> GamesaveReader::open(const QString &path, const GamesaveLimits &limits)
{
    auto impl = std::make_unique<Impl>();
    impl->file.setFileName(path);
    if (!impl->file.exists())
        return Error{ErrorCode::NotFound, QStringLiteral("file not found"), path};
    if (!impl->file.open(QIODevice::ReadOnly))
        return Error{ErrorCode::Io, impl->file.errorString(), path};

    impl->zip.m_pRead = &readFromFile;
    impl->zip.m_pIO_opaque = &impl->file;
    if (!mz_zip_reader_init(&impl->zip, static_cast<mz_uint64>(impl->file.size()), 0))
        return invalid(QStringLiteral("not a .gamesave file (not a valid ZIP archive)"), path);
    impl->zipOpen = true;

    const mz_uint count = mz_zip_reader_get_num_files(&impl->zip);
    if (count < 3)
        return invalid(QStringLiteral("not a .gamesave file (entries missing)"), path);

    QSet<QString> seenNames;
    qint64 total = 0;
    mz_uint manifestIndex = count;
    mz_uint hashIndex = count;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&impl->zip, i, &stat))
            return invalid(QStringLiteral("an archive entry is damaged"), QString::number(i));
        const QString name = QString::fromUtf8(stat.m_filename);

        // Rule 1: mimetype first, stored, exact content.
        if (i == 0) {
            if (name != kMimetypeEntry || stat.m_method != 0 || stat.m_uncomp_size != static_cast<mz_uint64>(kMimetype.size()))
                return invalid(QStringLiteral("not a .gamesave file (mimetype entry)"), path);
            auto mime = impl->extract(0, kMimetype.size());
            if (!mime.ok() || mime.value() != kMimetype)
                return invalid(QStringLiteral("not a .gamesave file (mimetype content)"), path);
        }
        // Rule 2: safe, unique names.
        if (!safeEntryName(name))
            return invalid(QStringLiteral("unsafe entry name in the archive"), name);
        if (seenNames.contains(name.toLower()))
            return invalid(QStringLiteral("duplicate entry in the archive"), name);
        seenNames.insert(name.toLower());
        // Rule 3: no encrypted, directory or special entries.
        const mz_uint32 unixMode = stat.m_external_attr >> 16;
        const mz_uint32 type = unixMode & 0170000;
        if (stat.m_is_encrypted || stat.m_is_directory || (type != 0 && type != 0100000) || !stat.m_is_supported)
            return invalid(QStringLiteral("unsupported entry in the archive"), name);
        // Rule 4: resource limits.
        total += static_cast<qint64>(stat.m_uncomp_size);
        if (total > limits.maxTotalBytes)
            return invalid(QStringLiteral("the archive is larger than the allowed limit"), path);
        if (stat.m_uncomp_size > 0
            && static_cast<qint64>(stat.m_uncomp_size / qMax<mz_uint64>(1, stat.m_comp_size)) > limits.maxCompressionRatio)
            return invalid(QStringLiteral("an entry is compressed suspiciously well"), name);

        if (i == 0)
            continue;
        if (name == kManifestEntry)
            manifestIndex = i;
        else if (name == kManifestHashEntry)
            hashIndex = i;
        else if (name.startsWith(kBlobPrefix) && sha256Pattern().match(name.mid(kBlobPrefix.size())).hasMatch())
            impl->blobIndex.insert(name.mid(kBlobPrefix.size()), i);
        else if (name != kReadmeEntry)
            return invalid(QStringLiteral("unexpected entry in the archive"), name);
    }
    if (manifestIndex == count || hashIndex == count)
        return invalid(QStringLiteral("not a .gamesave file (manifest missing)"), path);

    // Rule 5: manifest hash and content.
    mz_zip_archive_file_stat stat;
    mz_zip_reader_file_stat(&impl->zip, manifestIndex, &stat);
    if (stat.m_uncomp_size > 256ULL * 1024 * 1024)
        return invalid(QStringLiteral("the manifest is too large"), path);
    auto manifestJson = impl->extract(manifestIndex, static_cast<qint64>(stat.m_uncomp_size));
    if (!manifestJson.ok())
        return manifestJson.error();
    mz_zip_reader_file_stat(&impl->zip, hashIndex, &stat);
    if (stat.m_uncomp_size > 65)
        return invalid(QStringLiteral("the manifest hash is malformed"), path);
    auto hash = impl->extract(hashIndex, static_cast<qint64>(stat.m_uncomp_size));
    if (!hash.ok())
        return hash.error();
    QByteArray declared = hash.value();
    if (declared.endsWith('\n'))
        declared.chop(1);
    if (QString::fromLatin1(declared) != sha256Hex(manifestJson.value()))
        return invalid(QStringLiteral("the manifest does not match its hash"), path);
    auto manifest = parseManifest(manifestJson.value());
    if (!manifest.ok())
        return manifest.error();
    impl->manifest = std::move(manifest).value();

    // Rule 6: every blob hashes to its name; full exports carry every blob.
    for (auto it = impl->blobIndex.cbegin(); it != impl->blobIndex.cend(); ++it) {
        mz_zip_reader_file_stat(&impl->zip, it.value(), &stat);
        auto bytes = impl->extract(it.value(), static_cast<qint64>(stat.m_uncomp_size));
        if (!bytes.ok())
            return bytes.error();
        if (sha256Hex(bytes.value()) != it.key())
            return invalid(QStringLiteral("a file in the archive does not match its hash"), it.key());
    }
    GamesaveReader reader(std::move(impl));
    if (!reader.manifest().incremental() && !reader.missingBlobs().isEmpty())
        return invalid(QStringLiteral("the archive is missing files its backups need"), *reader.missingBlobs().cbegin());
    return reader;
}

} // namespace saveforge::archive
