#include "saveforge/vault/Snapshot.h"

#include "saveforge/core/JsonReader.h"
#include "saveforge/core/Uuid.h"
#include "saveforge/profiles/PathRules.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <limits>

namespace saveforge::vault {

namespace {

QString child(const QString &p, const QString &k) { return JsonReader::child(p, k); }
QString child(const QString &p, qsizetype i) { return JsonReader::child(p, i); }

const QRegularExpression &uuidPattern()
{
    static const QRegularExpression re(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    return re;
}

const QRegularExpression &timestampPattern()
{
    static const QRegularExpression re(
        QStringLiteral("^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}(\\.\\d+)?(Z|[+-]\\d{2}:\\d{2})$"));
    return re;
}

const QRegularExpression &slugPattern()
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9]+(-[a-z0-9]+)*$"));
    return re;
}

const QRegularExpression &gameIdPattern()
{
    static const QRegularExpression re(QStringLiteral(
        "^([a-z0-9]+(-[a-z0-9]+)*|local:[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$"));
    return re;
}

// vault/SPEC.md `slot`: no control characters, no leading or trailing whitespace.
const QRegularExpression &slotPattern()
{
    static const QRegularExpression re(QStringLiteral("^[^\\s\\x00-\\x1F](?:[^\\x00-\\x1F]*[^\\s\\x00-\\x1F])?$"),
                                       QRegularExpression::UseUnicodePropertiesOption);
    return re;
}

const QRegularExpression &semverPattern()
{
    static const QRegularExpression re(
        QStringLiteral("^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)(-[0-9A-Za-z.-]+)?$"));
    return re;
}

const QRegularExpression &sha256Pattern()
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-f]{64}$"));
    return re;
}

const QRegularExpression &concreteTemplatePattern()
{
    // templatePath from common/defs plus "no globs".
    static const QRegularExpression re(QStringLiteral("^<[A-Za-z]+>(/[^/*?]+)*$"));
    return re;
}

const QRegularExpression &base64Pattern()
{
    static const QRegularExpression re(
        QStringLiteral("^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$"));
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
    return !out.isNull() || r.fail(pointer, QStringLiteral("must be a UUID"));
}

bool readTimestamp(JsonReader &r, const QJsonValue &value, const QString &pointer, QDateTime &out)
{
    QString text;
    if (!r.string(value, pointer, text, 20, 40, &timestampPattern()))
        return false;
    out = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!out.isValid())
        return r.fail(pointer, QStringLiteral("must be an RFC 3339 date-time"));
    out = out.toUTC();
    return true;
}

const QStringList kKinds{QStringLiteral("manual"), QStringLiteral("auto"), QStringLiteral("pre-restore"),
                         QStringLiteral("import")};
const QStringList kTriggers{QStringLiteral("game-exit"), QStringLiteral("schedule"), QStringLiteral("file-change")};
const QStringList kChannels{QStringLiteral("official"), QStringLiteral("upstream-ludusavi"),
                            QStringLiteral("community-experimental"), QStringLiteral("local")};
const QStringList kRegistryTypes{QStringLiteral("REG_SZ"), QStringLiteral("REG_EXPAND_SZ"),
                                 QStringLiteral("REG_MULTI_SZ"), QStringLiteral("REG_DWORD"),
                                 QStringLiteral("REG_QWORD"), QStringLiteral("REG_BINARY")};

void readFile(JsonReader &r, const QJsonValue &value, const QString &ptr, SnapshotFile &out)
{
    QJsonObject o;
    if (!r.object(value, ptr, o)
        || !r.keys(o, ptr, {"path", "bindings", "ruleId", "sha256", "size", "mtime"},
                   {"path", "ruleId", "sha256", "size", "mtime"}))
        return;

    const QString pathPtr = child(ptr, QStringLiteral("path"));
    if (r.string(o.value(QLatin1String("path")), pathPtr, out.path, 3, 512, &concreteTemplatePattern())) {
        // Snapshot paths are concrete: syntax, placeholder and traversal rules apply (P5/P6 do not).
        QStringList violations = profiles::PathRules::builtin().checkSavePath(out.path).violations;
        violations.removeIf([](const QString &rule) { return rule == u"P5" || rule == u"P6"; });
        if (!violations.isEmpty())
            r.fail(pathPtr, QStringLiteral("unsafe path (violates %1)").arg(violations.join(QLatin1String(", "))));
    }

    if (o.contains(QLatin1String("bindings"))) {
        const QString bindingsPtr = child(ptr, QStringLiteral("bindings"));
        QJsonObject bindings;
        if (r.object(o.value(QLatin1String("bindings")), bindingsPtr, bindings)
            && r.keys(bindings, bindingsPtr, {"storeUserId", "storeGameId", "osUserName"})) {
            for (auto it = bindings.begin(); it != bindings.end(); ++it) {
                QString bound;
                const qsizetype maxLength = it.key() == u"osUserName" ? 256 : 64;
                if (r.string(it.value(), child(bindingsPtr, it.key()), bound, 1, maxLength))
                    out.bindings.insert(it.key(), bound);
            }
        }
    }

    // Every segment placeholder in the path needs its source value.
    static const QRegularExpression token(QStringLiteral("<([^<>]+)>"));
    auto it = token.globalMatch(out.path);
    bool first = true;
    while (it.hasNext() && !r.failed()) {
        const auto match = it.next();
        if (first) { first = false; continue; } // root placeholder
        if (!out.bindings.contains(match.captured(1)))
            r.fail(pathPtr, QStringLiteral("no binding for %1").arg(match.captured(0)));
    }

    r.string(o.value(QLatin1String("ruleId")), child(ptr, QStringLiteral("ruleId")), out.ruleId, 1, 100, &slugPattern());
    r.string(o.value(QLatin1String("sha256")), child(ptr, QStringLiteral("sha256")), out.sha256, 64, 64, &sha256Pattern());
    r.integer(o.value(QLatin1String("size")), child(ptr, QStringLiteral("size")), out.size, 0);
    readTimestamp(r, o.value(QLatin1String("mtime")), child(ptr, QStringLiteral("mtime")), out.mtime);
}

void readRegistryValue(JsonReader &r, const QJsonValue &value, const QString &ptr, RegistryValue &out)
{
    QJsonObject o;
    if (!r.object(value, ptr, o) || !r.keys(o, ptr, {"name", "type", "data"}, {"name", "type", "data"}))
        return;
    r.string(o.value(QLatin1String("name")), child(ptr, QStringLiteral("name")), out.name, 0, 16383);
    if (!r.enumeration(o.value(QLatin1String("type")), child(ptr, QStringLiteral("type")), out.type, kRegistryTypes))
        return;

    const QString dataPtr = child(ptr, QStringLiteral("data"));
    out.data = o.value(QLatin1String("data"));
    QString text;
    if (out.type == u"REG_SZ" || out.type == u"REG_EXPAND_SZ") {
        r.string(out.data, dataPtr, text);
    } else if (out.type == u"REG_MULTI_SZ") {
        QStringList items;
        r.stringList(out.data, dataPtr, items, 0, JsonReader::kUnbounded, 0, JsonReader::kUnbounded, nullptr, false);
    } else if (out.type == u"REG_DWORD") {
        qint64 dword = 0;
        r.integer(out.data, dataPtr, dword, 0, 4294967295LL);
    } else if (out.type == u"REG_QWORD") {
        static const QRegularExpression decimal(QStringLiteral("^(0|[1-9][0-9]{0,19})$"));
        r.string(out.data, dataPtr, text, 1, 20, &decimal);
    } else {
        r.string(out.data, dataPtr, text, 0, JsonReader::kUnbounded, &base64Pattern());
    }
}

void readRegistryKey(JsonReader &r, const QJsonValue &value, const QString &ptr, RegistryKey &out)
{
    QJsonObject o;
    if (!r.object(value, ptr, o) || !r.keys(o, ptr, {"key", "ruleId", "values"}, {"key", "ruleId", "values"}))
        return;
    const QString keyPtr = child(ptr, QStringLiteral("key"));
    if (r.string(o.value(QLatin1String("key")), keyPtr, out.key, 1, 256)) {
        const auto check = profiles::PathRules::builtin().checkRegistryKey(out.key);
        if (!check.valid())
            r.fail(keyPtr, QStringLiteral("unsafe registry key (violates %1)").arg(check.violations.join(QLatin1String(", "))));
    }
    r.string(o.value(QLatin1String("ruleId")), child(ptr, QStringLiteral("ruleId")), out.ruleId, 1, 100, &slugPattern());

    const QString valuesPtr = child(ptr, QStringLiteral("values"));
    QJsonArray values;
    if (!r.array(o.value(QLatin1String("values")), valuesPtr, values))
        return;
    for (qsizetype i = 0; i < values.size() && !r.failed(); ++i) {
        RegistryValue entry;
        readRegistryValue(r, values.at(i), child(valuesPtr, i), entry);
        out.values.append(entry);
    }
}

} // namespace

QString gameDirectoryName(const QString &gameId)
{
    QString name = gameId;
    name.replace(u':', u'_');
    return name;
}

QByteArray toJson(const Snapshot &s)
{
    QJsonObject o;
    o.insert(QStringLiteral("schemaVersion"), 1);
    o.insert(QStringLiteral("type"), QStringLiteral("snapshot"));
    o.insert(QStringLiteral("snapshotId"), uuidToString(s.snapshotId));
    o.insert(QStringLiteral("parentId"), s.parentId ? QJsonValue(uuidToString(*s.parentId)) : QJsonValue());
    o.insert(QStringLiteral("gameId"), s.gameId);
    o.insert(QStringLiteral("createdAt"), timestamp(s.createdAt));
    o.insert(QStringLiteral("kind"), kKinds.at(static_cast<qsizetype>(s.kind)));
    if (s.trigger)
        o.insert(QStringLiteral("trigger"), kTriggers.at(static_cast<qsizetype>(*s.trigger)));
    if (!s.label.isEmpty())
        o.insert(QStringLiteral("label"), s.label);
    if (!s.slot.isEmpty())
        o.insert(QStringLiteral("slot"), s.slot);
    if (s.pinned)
        o.insert(QStringLiteral("pinned"), true);
    o.insert(QStringLiteral("machineId"), uuidToString(s.machineId));
    o.insert(QStringLiteral("appVersion"), s.appVersion);
    o.insert(QStringLiteral("profile"), QJsonObject{{QStringLiteral("channel"), s.profile.channel},
                                                    {QStringLiteral("id"), s.profile.id},
                                                    {QStringLiteral("revision"), s.profile.revision}});
    o.insert(QStringLiteral("consistency"),
             s.consistency == Consistency::Consistent ? QStringLiteral("consistent") : QStringLiteral("inconsistent"));

    QJsonArray files;
    for (const SnapshotFile &f : s.files) {
        QJsonObject file{{QStringLiteral("path"), f.path},
                         {QStringLiteral("ruleId"), f.ruleId},
                         {QStringLiteral("sha256"), f.sha256},
                         {QStringLiteral("size"), f.size},
                         {QStringLiteral("mtime"), timestamp(f.mtime)}};
        if (!f.bindings.isEmpty()) {
            QJsonObject bindings;
            for (auto it = f.bindings.cbegin(); it != f.bindings.cend(); ++it)
                bindings.insert(it.key(), it.value());
            file.insert(QStringLiteral("bindings"), bindings);
        }
        files.append(file);
    }
    o.insert(QStringLiteral("files"), files);

    if (!s.registry.isEmpty()) {
        QJsonArray registry;
        for (const RegistryKey &key : s.registry) {
            QJsonArray values;
            for (const RegistryValue &v : key.values)
                values.append(QJsonObject{{QStringLiteral("name"), v.name},
                                          {QStringLiteral("type"), v.type},
                                          {QStringLiteral("data"), v.data}});
            registry.append(QJsonObject{{QStringLiteral("key"), key.key},
                                        {QStringLiteral("ruleId"), key.ruleId},
                                        {QStringLiteral("values"), values}});
        }
        o.insert(QStringLiteral("registry"), registry);
    }
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

QByteArray toJson(const VaultDescriptor &d)
{
    const QJsonObject o{{QStringLiteral("schemaVersion"), 1},
                        {QStringLiteral("type"), QStringLiteral("vault")},
                        {QStringLiteral("vaultId"), uuidToString(d.vaultId)},
                        {QStringLiteral("createdAt"), timestamp(d.createdAt)},
                        {QStringLiteral("hash"), QStringLiteral("sha256")},
                        {QStringLiteral("compression"), QStringLiteral("zstd")}};
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

Result<Snapshot> parseSnapshot(const QJsonObject &json)
{
    JsonReader r;
    Snapshot s;
    r.keys(json, QString(),
           {"schemaVersion", "type", "snapshotId", "parentId", "gameId", "createdAt", "kind", "trigger", "label",
            "slot", "pinned", "machineId", "appVersion", "profile", "consistency", "files", "registry"},
           {"schemaVersion", "type", "snapshotId", "gameId", "createdAt", "kind", "machineId", "appVersion",
            "profile", "consistency", "files"});
    if (r.failed())
        return r.error();

    qint64 schemaVersion = 0;
    if (r.integer(json.value(QLatin1String("schemaVersion")), QStringLiteral("/schemaVersion"), schemaVersion, 1)
        && schemaVersion != 1) {
        return Error{ErrorCode::Unsupported, QStringLiteral("snapshot schemaVersion %1 is not supported").arg(schemaVersion),
                     QStringLiteral("/schemaVersion")};
    }
    QString type;
    r.enumeration(json.value(QLatin1String("type")), QStringLiteral("/type"), type, {QStringLiteral("snapshot")});
    readUuid(r, json.value(QLatin1String("snapshotId")), QStringLiteral("/snapshotId"), s.snapshotId);
    if (json.contains(QLatin1String("parentId")) && !json.value(QLatin1String("parentId")).isNull()) {
        QUuid parent;
        if (readUuid(r, json.value(QLatin1String("parentId")), QStringLiteral("/parentId"), parent))
            s.parentId = parent;
    }
    r.string(json.value(QLatin1String("gameId")), QStringLiteral("/gameId"), s.gameId, 1, 100, &gameIdPattern());
    readTimestamp(r, json.value(QLatin1String("createdAt")), QStringLiteral("/createdAt"), s.createdAt);

    QString kind;
    if (r.enumeration(json.value(QLatin1String("kind")), QStringLiteral("/kind"), kind, kKinds))
        s.kind = static_cast<SnapshotKind>(kKinds.indexOf(kind));
    if (json.contains(QLatin1String("trigger"))) {
        QString trigger;
        if (r.enumeration(json.value(QLatin1String("trigger")), QStringLiteral("/trigger"), trigger, kTriggers))
            s.trigger = static_cast<SnapshotTrigger>(kTriggers.indexOf(trigger));
    }
    if (json.contains(QLatin1String("label")))
        r.string(json.value(QLatin1String("label")), QStringLiteral("/label"), s.label, 1, 200);
    if (json.contains(QLatin1String("slot")))
        r.string(json.value(QLatin1String("slot")), QStringLiteral("/slot"), s.slot, 1, 100, &slotPattern());
    if (json.contains(QLatin1String("pinned")))
        r.boolean(json.value(QLatin1String("pinned")), QStringLiteral("/pinned"), s.pinned);
    readUuid(r, json.value(QLatin1String("machineId")), QStringLiteral("/machineId"), s.machineId);
    r.string(json.value(QLatin1String("appVersion")), QStringLiteral("/appVersion"), s.appVersion, 5, 64, &semverPattern());

    QJsonObject profile;
    if (r.object(json.value(QLatin1String("profile")), QStringLiteral("/profile"), profile)
        && r.keys(profile, QStringLiteral("/profile"), {"channel", "id", "revision"}, {"channel", "id", "revision"})) {
        r.enumeration(profile.value(QLatin1String("channel")), QStringLiteral("/profile/channel"), s.profile.channel, kChannels);
        r.string(profile.value(QLatin1String("id")), QStringLiteral("/profile/id"), s.profile.id, 1, 100, &slugPattern());
        qint64 revision = 0;
        if (r.integer(profile.value(QLatin1String("revision")), QStringLiteral("/profile/revision"), revision, 1, std::numeric_limits<int>::max()))
            s.profile.revision = static_cast<int>(revision);
    }

    QString consistency;
    if (r.enumeration(json.value(QLatin1String("consistency")), QStringLiteral("/consistency"), consistency,
                      {QStringLiteral("consistent"), QStringLiteral("inconsistent")}))
        s.consistency = consistency == u"consistent" ? Consistency::Consistent : Consistency::Inconsistent;

    QJsonArray files;
    if (r.array(json.value(QLatin1String("files")), QStringLiteral("/files"), files)) {
        for (qsizetype i = 0; i < files.size() && !r.failed(); ++i) {
            SnapshotFile file;
            readFile(r, files.at(i), child(QStringLiteral("/files"), i), file);
            s.files.append(file);
        }
    }

    if (json.contains(QLatin1String("registry"))) {
        QJsonArray registry;
        if (r.array(json.value(QLatin1String("registry")), QStringLiteral("/registry"), registry)) {
            for (qsizetype i = 0; i < registry.size() && !r.failed(); ++i) {
                RegistryKey key;
                readRegistryKey(r, registry.at(i), child(QStringLiteral("/registry"), i), key);
                s.registry.append(key);
            }
        }
    }

    if (r.failed())
        return r.error();
    return s;
}

Result<Snapshot> parseSnapshot(const QByteArray &json)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return Error{ErrorCode::Corrupted, QStringLiteral("snapshot is not a JSON object"), parseError.errorString()};
    return parseSnapshot(doc.object());
}

Result<VaultDescriptor> parseVaultDescriptor(const QByteArray &json)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return Error{ErrorCode::Corrupted, QStringLiteral("vault.json is not a JSON object"), parseError.errorString()};

    const QJsonObject o = doc.object();
    JsonReader r;
    VaultDescriptor d;
    r.keys(o, QString(), {"schemaVersion", "type", "vaultId", "createdAt", "hash", "compression"},
           {"schemaVersion", "type", "vaultId", "createdAt", "hash", "compression"});
    qint64 schemaVersion = 0;
    if (r.integer(o.value(QLatin1String("schemaVersion")), QStringLiteral("/schemaVersion"), schemaVersion, 1)
        && schemaVersion != 1) {
        return Error{ErrorCode::Unsupported, QStringLiteral("vault layout version %1 is not supported").arg(schemaVersion),
                     QStringLiteral("/schemaVersion")};
    }
    QString value;
    r.enumeration(o.value(QLatin1String("type")), QStringLiteral("/type"), value, {QStringLiteral("vault")});
    readUuid(r, o.value(QLatin1String("vaultId")), QStringLiteral("/vaultId"), d.vaultId);
    readTimestamp(r, o.value(QLatin1String("createdAt")), QStringLiteral("/createdAt"), d.createdAt);
    r.enumeration(o.value(QLatin1String("hash")), QStringLiteral("/hash"), value, {QStringLiteral("sha256")});
    r.enumeration(o.value(QLatin1String("compression")), QStringLiteral("/compression"), value, {QStringLiteral("zstd")});
    if (r.failed())
        return r.error();
    return d;
}

bool isValidSlotName(const QString &name)
{
    return name.size() <= 100 && slotPattern().match(name).hasMatch();
}

} // namespace saveforge::vault
