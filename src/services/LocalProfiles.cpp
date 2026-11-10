#include "savevault/services/LocalProfiles.h"

#include "savevault/core/Uuid.h"
#include "savevault/platform/FileSync.h"
#include "savevault/profiles/ProfileParser.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace savevault::services {

namespace {

// Local profiles are never published; the user owns their data.
const QString kLocalLicense = QStringLiteral("LicenseRef-Local");
constexpr qint64 kMaxProfileBytes = 1024 * 1024;

// "Hollow Knight: Silksong" -> "hollow-knight-silksong".
QString slugify(const QString &name)
{
    const QString ascii = name.normalized(QString::NormalizationForm_KD).toLower();
    QString slug;
    bool dash = false;
    for (const QChar c : ascii) {
        if ((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9')) {
            if (dash && !slug.isEmpty())
                slug += u'-';
            slug += c;
            dash = false;
        } else if (c.category() != QChar::Mark_NonSpacing) {
            dash = true;
        }
    }
    return slug.left(80);
}

// Expresses `folder` as "<placeholder>/rest" through the deepest user folder
// that contains it.
Result<QString> templateFor(const QString &folder, const platform::FolderMap &userFolders, platform::Os os)
{
    const QString path = QDir::cleanPath(QDir::fromNativeSeparators(folder));
    const Qt::CaseSensitivity cs = os == platform::Os::Linux ? Qt::CaseSensitive : Qt::CaseInsensitive;
    QString best;
    QString bestDir;
    for (const QString &placeholder : userFolders.placeholders()) {
        const QString dir = userFolders.value(placeholder);
        if (dir.isEmpty() || dir.size() <= bestDir.size())
            continue;
        if (path.compare(dir, cs) == 0 || path.startsWith(dir + u'/', cs)) {
            best = placeholder;
            bestDir = dir;
        }
    }
    if (best.isEmpty())
        return Error{ErrorCode::InvalidArgument,
                     QStringLiteral("the folder is outside the user folders profiles can describe"), folder};
    const QString rest = path.mid(bestDir.size());
    // Glob characters in real folder names would change what the path matches.
    if (rest.contains(u'*') || rest.contains(u'?'))
        return Error{ErrorCode::InvalidArgument, QStringLiteral("folder names with * or ? are not supported"), folder};
    return u'<' + best + u'>' + rest;
}

} // namespace

LocalProfileStore::LocalProfileStore(const QString &directory)
    : m_dir(QDir::cleanPath(directory))
{
}

LocalProfileStore::Load LocalProfileStore::load() const
{
    Load result;
    const QStringList files = QDir(m_dir).entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &name : files) {
        QFile file(m_dir + u'/' + name);
        if (file.size() > kMaxProfileBytes || !file.open(QIODevice::ReadOnly)) {
            result.warnings.append(name + QStringLiteral(": cannot be read"));
            continue;
        }
        auto profile = profiles::parseProfile(file.readAll());
        if (!profile.ok()) {
            result.warnings.append(name + QStringLiteral(": ") + profile.error().toString());
            continue;
        }
        if (profile.value().id + QStringLiteral(".json") != name) {
            result.warnings.append(name + QStringLiteral(": file name does not match the profile id"));
            continue;
        }
        result.profiles.append(std::move(profile).value());
    }
    return result;
}

QString LocalProfileStore::uniqueId(const QString &name) const
{
    QString base = slugify(name);
    base = QStringLiteral("local") + (base.isEmpty() ? QString() : u'-' + base);
    QString id = base;
    for (int n = 2; QFile::exists(m_dir + u'/' + id + QStringLiteral(".json")); ++n)
        id = base + u'-' + QString::number(n);
    return id;
}

Result<profiles::Profile> LocalProfileStore::addManualGame(const QString &name, const QStringList &saveFolders,
                                                           const platform::FolderMap &userFolders, platform::Os os)
{
    const QString trimmed = name.simplified();
    if (trimmed.isEmpty())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("the game needs a name"), {}};
    if (saveFolders.isEmpty())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("choose at least one save folder"), {}};

    QJsonArray paths;
    for (const QString &folder : saveFolders) {
        auto path = templateFor(folder, userFolders, os);
        if (!path.ok())
            return path.error();
        if (!paths.contains(path.value()))
            paths.append(path.value());
    }

    const QString id = uniqueId(trimmed);
    // A folder name cannot contain path separators; anything else is kept.
    QString dirName = trimmed;
    dirName.replace(u'/', u' ').replace(u'\\', u' ');
    const QJsonObject json{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), trimmed},
        {QStringLiteral("revision"), 1},
        {QStringLiteral("identifiers"), QJsonObject{{QStringLiteral("installDirNames"), QJsonArray{dirName.simplified()}}}},
        {QStringLiteral("saves"), QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("saves")},
                                                         {QStringLiteral("tags"), QJsonArray{QStringLiteral("save")}},
                                                         {QStringLiteral("paths"), paths}}}},
        {QStringLiteral("mechanism"), QJsonObject{{QStringLiteral("type"), QStringLiteral("standard")}}},
        {QStringLiteral("provenance"), QJsonObject{{QStringLiteral("source"), QStringLiteral("local")},
                                                   {QStringLiteral("license"), kLocalLicense}}},
    };
    // The same validation as any registry profile, including the path rules.
    auto profile = profiles::parseProfile(json);
    if (!profile.ok())
        return profile.error();
    if (auto written = write(id, QJsonDocument(json).toJson()); !written.ok())
        return written.error();
    return profile;
}

Result<profiles::Profile> LocalProfileStore::importProfile(const QString &file)
{
    QFile source(file);
    if (!source.exists())
        return Error{ErrorCode::NotFound, QStringLiteral("file not found"), file};
    if (source.size() > kMaxProfileBytes)
        return Error{ErrorCode::InvalidArgument, QStringLiteral("file is too large for a profile"), file};
    if (!source.open(QIODevice::ReadOnly))
        return Error{ErrorCode::Io, source.errorString(), file};
    return importProfileJson(source.readAll());
}

Result<profiles::Profile> LocalProfileStore::importProfileJson(const QByteArray &json)
{
    if (json.size() > kMaxProfileBytes)
        return Error{ErrorCode::InvalidArgument, QStringLiteral("too large for a profile"), {}};
    auto profile = profiles::parseProfile(json);
    if (!profile.ok())
        return profile.error();
    if (auto written = write(profile.value().id, json); !written.ok())
        return written.error();
    return profile;
}

Result<QByteArray> LocalProfileStore::profileJson(const QString &profileId) const
{
    QFile file(m_dir + u'/' + profileId + QStringLiteral(".json"));
    if (!file.exists())
        return Error{ErrorCode::NotFound, QStringLiteral("no local profile with this id"), profileId};
    if (!file.open(QIODevice::ReadOnly))
        return Error{ErrorCode::Io, file.errorString(), file.fileName()};
    return file.readAll();
}

Result<void> LocalProfileStore::remove(const QString &profileId)
{
    static const QRegularExpression slug(QStringLiteral("^[a-z0-9]+(-[a-z0-9]+)*$"));
    const QString path = m_dir + u'/' + profileId + QStringLiteral(".json");
    if (!slug.match(profileId).hasMatch() || !QFile::exists(path))
        return Error{ErrorCode::NotFound, QStringLiteral("no local profile with this id"), profileId};
    if (!QFile::remove(path))
        return Error{ErrorCode::Io, QStringLiteral("cannot delete the profile"), path};
    return {};
}

Result<void> LocalProfileStore::write(const QString &profileId, const QByteArray &json)
{
    if (!QDir().mkpath(m_dir))
        return Error{ErrorCode::Io, QStringLiteral("cannot create the profiles folder"), m_dir};
    const QString target = m_dir + u'/' + profileId + QStringLiteral(".json");
    const QString tmp = target + u'.' + uuidToString(createUuidV7()).right(12) + QStringLiteral(".tmp");
    QFile out(tmp);
    if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly) || out.write(json) != json.size() || !platform::syncFile(out)) {
        out.close();
        QFile::remove(tmp);
        return Error{ErrorCode::Io, QStringLiteral("cannot write the profile"), tmp};
    }
    out.close();
    QFile::remove(target);
    if (!QFile::rename(tmp, target)) {
        QFile::remove(tmp);
        return Error{ErrorCode::Io, QStringLiteral("cannot store the profile"), target};
    }
    platform::syncDirectory(m_dir);
    return {};
}

} // namespace savevault::services
