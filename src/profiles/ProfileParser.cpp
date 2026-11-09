#include "saveforge/profiles/ProfileParser.h"

#include "saveforge/core/JsonReader.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <limits>

namespace saveforge::profiles {

namespace {

// Patterns mirror profile.v1.schema.json / common/defs.v1.schema.json.
const QRegularExpression &slugPattern()
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9]+(-[a-z0-9]+)*$"));
    return re;
}

const QRegularExpression &semverPattern()
{
    static const QRegularExpression re(
        QStringLiteral("^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)(-[0-9A-Za-z.-]+)?$"));
    return re;
}

const QRegularExpression &templatePathPattern()
{
    static const QRegularExpression re(QStringLiteral("^<[A-Za-z]+>(/[^/]+)*$"));
    return re;
}

const QRegularExpression &bareNamePattern()
{
    static const QRegularExpression re(QStringLiteral("^[^/\\\\]+$"));
    return re;
}

const QRegularExpression &contributorPattern()
{
    static const QRegularExpression re(QStringLiteral("^(community|github):[A-Za-z0-9_.-]+$"));
    return re;
}

const QStringList &stores()
{
    static const QStringList values{
        QStringLiteral("steam"), QStringLiteral("epic"), QStringLiteral("gog"), QStringLiteral("heroic"),
        QStringLiteral("lutris"), QStringLiteral("itch"), QStringLiteral("ea"), QStringLiteral("ubisoft"),
        QStringLiteral("battlenet"), QStringLiteral("amazon"), QStringLiteral("manual"), QStringLiteral("other")};
    return values;
}

constexpr qint64 kMaxInt = std::numeric_limits<qint64>::max();

using Reader = JsonReader;

QString child(const QString &pointer, const QString &key)
{
    return JsonReader::child(pointer, key);
}

QString child(const QString &pointer, qsizetype index)
{
    return JsonReader::child(pointer, index);
}

void readTags(Reader &r, const QJsonValue &value, const QString &pointer, QList<Tag> &out)
{
    static const QStringList allowed{QStringLiteral("save"), QStringLiteral("config")};
    QStringList names;
    if (!r.stringList(value, pointer, names, 1, 2, 1, 16, nullptr, true, &allowed))
        return;
    for (const QString &name : names)
        out.append(name == u"save" ? Tag::Save : Tag::Config);
}

void readIdentifiers(Reader &r, const QJsonValue &value, Identifiers &out)
{
    const QString ptr = QStringLiteral("/identifiers");
    QJsonObject o;
    if (!r.object(value, ptr, o) || !r.keys(o, ptr, {"steam", "gog", "epic", "executables", "installDirNames"}))
        return;
    if (o.isEmpty()) {
        r.fail(ptr, QStringLiteral("must have at least 1 property"));
        return;
    }
    constexpr qsizetype any = std::numeric_limits<qsizetype>::max();
    if (o.contains(QLatin1String("steam")))
        r.integerList(o.value(QLatin1String("steam")), child(ptr, QStringLiteral("steam")), out.steam);
    if (o.contains(QLatin1String("gog")))
        r.integerList(o.value(QLatin1String("gog")), child(ptr, QStringLiteral("gog")), out.gog);
    if (o.contains(QLatin1String("epic")))
        r.stringList(o.value(QLatin1String("epic")), child(ptr, QStringLiteral("epic")), out.epic, 0, any, 1, 128);
    if (o.contains(QLatin1String("executables")))
        r.stringList(o.value(QLatin1String("executables")), child(ptr, QStringLiteral("executables")),
                     out.executables, 0, any, 1, 128, &bareNamePattern());
    if (o.contains(QLatin1String("installDirNames")))
        r.stringList(o.value(QLatin1String("installDirNames")), child(ptr, QStringLiteral("installDirNames")),
                     out.installDirNames, 0, any, 1, 128, &bareNamePattern());
}

void readCondition(Reader &r, const QJsonValue &value, const QString &ptr, Condition &out)
{
    QJsonObject o;
    if (!r.object(value, ptr, o) || !r.keys(o, ptr, {"os", "store"}))
        return;
    if (o.contains(QLatin1String("os"))) {
        static const QStringList oses{QStringLiteral("windows"), QStringLiteral("linux"), QStringLiteral("macos")};
        QString os;
        if (r.enumeration(o.value(QLatin1String("os")), child(ptr, QStringLiteral("os")), os, oses))
            out.os = platform::osFromName(os);
    }
    if (o.contains(QLatin1String("store")))
        r.enumeration(o.value(QLatin1String("store")), child(ptr, QStringLiteral("store")), out.store, stores());
}

void readSaveRules(Reader &r, const QJsonValue &value, QList<SaveRule> &out)
{
    const QString ptr = QStringLiteral("/saves");
    QJsonArray items;
    if (!r.array(value, ptr, items, 1, 64))
        return;
    for (qsizetype i = 0; i < items.size() && !r.failed(); ++i) {
        const QString itemPtr = child(ptr, i);
        QJsonObject o;
        if (!r.object(items.at(i), itemPtr, o) || !r.keys(o, itemPtr, {"id", "tags", "when", "paths"}, {"id", "tags", "paths"}))
            return;
        SaveRule rule;
        r.string(o.value(QLatin1String("id")), child(itemPtr, QStringLiteral("id")), rule.id, 1, 100, &slugPattern());
        readTags(r, o.value(QLatin1String("tags")), child(itemPtr, QStringLiteral("tags")), rule.tags);
        if (o.contains(QLatin1String("when")))
            readCondition(r, o.value(QLatin1String("when")), child(itemPtr, QStringLiteral("when")), rule.when);
        r.stringList(o.value(QLatin1String("paths")), child(itemPtr, QStringLiteral("paths")), rule.paths, 1, 32, 3, 512,
                     &templatePathPattern());
        out.append(rule);
    }
}

void readRegistryRules(Reader &r, const QJsonValue &value, QList<RegistryRule> &out)
{
    const QString ptr = QStringLiteral("/registry");
    QJsonArray items;
    if (!r.array(value, ptr, items, 1, 16))
        return;
    for (qsizetype i = 0; i < items.size() && !r.failed(); ++i) {
        const QString itemPtr = child(ptr, i);
        QJsonObject o;
        if (!r.object(items.at(i), itemPtr, o) || !r.keys(o, itemPtr, {"id", "tags", "keys"}, {"id", "tags", "keys"}))
            return;
        RegistryRule rule;
        r.string(o.value(QLatin1String("id")), child(itemPtr, QStringLiteral("id")), rule.id, 1, 100, &slugPattern());
        readTags(r, o.value(QLatin1String("tags")), child(itemPtr, QStringLiteral("tags")), rule.tags);
        r.stringList(o.value(QLatin1String("keys")), child(itemPtr, QStringLiteral("keys")), rule.keys, 1, 16, 1, 256);
        out.append(rule);
    }
}

void readBackup(Reader &r, const QJsonValue &value, BackupOptions &out)
{
    const QString ptr = QStringLiteral("/backup");
    QJsonObject o;
    if (!r.object(value, ptr, o)
        || !r.keys(o, ptr, {"requireGameClosed", "processNames", "cloudSync", "maxBytes", "consistentGroups"}))
        return;
    constexpr qsizetype any = std::numeric_limits<qsizetype>::max();
    if (o.contains(QLatin1String("requireGameClosed")))
        r.boolean(o.value(QLatin1String("requireGameClosed")), child(ptr, QStringLiteral("requireGameClosed")),
                  out.requireGameClosed);
    if (o.contains(QLatin1String("processNames")))
        r.stringList(o.value(QLatin1String("processNames")), child(ptr, QStringLiteral("processNames")),
                     out.processNames, 0, any, 1, 128);
    if (o.contains(QLatin1String("cloudSync")))
        r.stringList(o.value(QLatin1String("cloudSync")), child(ptr, QStringLiteral("cloudSync")), out.cloudSync, 0,
                     any, 1, 64, nullptr, true, &stores());
    if (o.contains(QLatin1String("maxBytes"))) {
        qint64 maxBytes = 0;
        if (r.integer(o.value(QLatin1String("maxBytes")), child(ptr, QStringLiteral("maxBytes")), maxBytes, 1, kMaxInt))
            out.maxBytes = maxBytes;
    }
    if (o.contains(QLatin1String("consistentGroups"))) {
        const QString groupsPtr = child(ptr, QStringLiteral("consistentGroups"));
        QJsonArray groups;
        if (!r.array(o.value(QLatin1String("consistentGroups")), groupsPtr, groups, 0, any))
            return;
        for (qsizetype i = 0; i < groups.size(); ++i) {
            QStringList group;
            if (!r.stringList(groups.at(i), child(groupsPtr, i), group, 2, any, 1, 256, nullptr, false))
                return;
            out.consistentGroups.append(group);
        }
    }
}

void readMechanism(Reader &r, const QJsonValue &value, Mechanism &out)
{
    const QString ptr = QStringLiteral("/mechanism");
    QJsonObject o;
    if (!r.object(value, ptr, o) || !r.keys(o, ptr, {"type", "params"}, {"type"}))
        return;
    r.string(o.value(QLatin1String("type")), child(ptr, QStringLiteral("type")), out.type, 1, 64, &slugPattern());
    if (o.contains(QLatin1String("params")))
        r.object(o.value(QLatin1String("params")), child(ptr, QStringLiteral("params")), out.params);
    if (r.failed())
        return;

    const QString paramsPtr = child(ptr, QStringLiteral("params"));
    const bool hasParams = o.contains(QLatin1String("params"));
    if (out.type == u"standard") {
        if (hasParams)
            r.fail(paramsPtr, QStringLiteral("must be omitted for mechanism 'standard'"));
    } else if (out.type == u"latest-n") {
        if (!hasParams) {
            r.fail(ptr, QStringLiteral("missing required property 'params'"));
            return;
        }
        if (!r.keys(out.params, paramsPtr, {"n", "sortBy"}, {"n"}))
            return;
        qint64 n = 0;
        r.integer(out.params.value(QLatin1String("n")), child(paramsPtr, QStringLiteral("n")), n, 1, 100);
        if (out.params.contains(QLatin1String("sortBy"))) {
            QString sortBy;
            r.enumeration(out.params.value(QLatin1String("sortBy")), child(paramsPtr, QStringLiteral("sortBy")), sortBy,
                          {QStringLiteral("mtime"), QStringLiteral("name")});
        }
    } else if (out.type == u"store-user-scoped") {
        if (!hasParams) {
            r.fail(ptr, QStringLiteral("missing required property 'params'"));
            return;
        }
        if (!r.keys(out.params, paramsPtr, {"store"}, {"store"}))
            return;
        QString store;
        r.enumeration(out.params.value(QLatin1String("store")), child(paramsPtr, QStringLiteral("store")), store,
                      {QStringLiteral("steam"), QStringLiteral("epic"), QStringLiteral("gog")});
    }
}

void readCompatibility(Reader &r, const QJsonValue &value, Profile &out)
{
    const QString ptr = QStringLiteral("/compatibility");
    QJsonObject o;
    if (!r.object(value, ptr, o) || !r.keys(o, ptr, {"minAppVersion", "gameVersions"}))
        return;
    if (o.contains(QLatin1String("minAppVersion")))
        r.string(o.value(QLatin1String("minAppVersion")), child(ptr, QStringLiteral("minAppVersion")),
                 out.minAppVersion, 5, 64, &semverPattern());
    if (o.contains(QLatin1String("gameVersions")))
        r.string(o.value(QLatin1String("gameVersions")), child(ptr, QStringLiteral("gameVersions")), out.gameVersions,
                 0, 64);
}

void readProvenance(Reader &r, const QJsonValue &value, Provenance &out)
{
    const QString ptr = QStringLiteral("/provenance");
    QJsonObject o;
    if (!r.object(value, ptr, o) || !r.keys(o, ptr, {"source", "license", "basedOn", "contributors"}, {"source", "license"}))
        return;
    r.enumeration(o.value(QLatin1String("source")), child(ptr, QStringLiteral("source")), out.source,
                  {QStringLiteral("saveforge"), QStringLiteral("ludusavi"), QStringLiteral("local")});
    r.string(o.value(QLatin1String("license")), child(ptr, QStringLiteral("license")), out.license, 1, 64);
    if (o.contains(QLatin1String("basedOn"))) {
        const QString basedOnPtr = child(ptr, QStringLiteral("basedOn"));
        QJsonObject basedOn;
        if (r.object(o.value(QLatin1String("basedOn")), basedOnPtr, basedOn) && r.keys(basedOn, basedOnPtr, {"ludusavi"})
            && basedOn.contains(QLatin1String("ludusavi"))) {
            r.string(basedOn.value(QLatin1String("ludusavi")), child(basedOnPtr, QStringLiteral("ludusavi")),
                     out.basedOnLudusavi, 1, 500);
        }
    }
    if (o.contains(QLatin1String("contributors")))
        r.stringList(o.value(QLatin1String("contributors")), child(ptr, QStringLiteral("contributors")),
                     out.contributors, 0, std::numeric_limits<qsizetype>::max(), 1, 200, &contributorPattern());
}

// Rules JSON Schema cannot express: path safety and OS coherence.
void checkSemantics(Reader &r, const Profile &profile, const PathRules &rules)
{
    for (qsizetype i = 0; i < profile.saves.size(); ++i) {
        const SaveRule &rule = profile.saves.at(i);
        for (qsizetype j = 0; j < rule.paths.size(); ++j) {
            const QString ptr = child(child(child(QStringLiteral("/saves"), i), QStringLiteral("paths")), j);
            const RuleCheck check = rules.checkSavePath(rule.paths.at(j));
            if (!check.valid()) {
                r.fail(ptr, QStringLiteral("unsafe path (violates %1)").arg(check.violations.join(QLatin1String(", "))));
                return;
            }
            const auto implied = rules.impliedOs(rule.paths.at(j));
            if (implied && rule.when.os && *implied != *rule.when.os) {
                r.fail(ptr, QStringLiteral("placeholder implies os '%1' but the rule is for '%2'")
                                .arg(platform::osName(*implied), platform::osName(*rule.when.os)));
                return;
            }
        }
    }
    for (qsizetype i = 0; i < profile.registry.size(); ++i) {
        const RegistryRule &rule = profile.registry.at(i);
        for (qsizetype j = 0; j < rule.keys.size(); ++j) {
            const RuleCheck check = rules.checkRegistryKey(rule.keys.at(j));
            if (!check.valid()) {
                r.fail(child(child(child(QStringLiteral("/registry"), i), QStringLiteral("keys")), j),
                       QStringLiteral("unsafe registry key (violates %1)").arg(check.violations.join(QLatin1String(", "))));
                return;
            }
        }
    }
}

} // namespace

const QStringList &supportedMechanisms()
{
    static const QStringList types{QStringLiteral("standard"), QStringLiteral("latest-n"),
                                   QStringLiteral("store-user-scoped")};
    return types;
}

Result<Profile> parseProfile(const QJsonObject &json, const PathRules &rules)
{
    Reader r;
    Profile profile;

    r.keys(json, QString(),
           {"schemaVersion", "id", "name", "revision", "identifiers", "saves", "registry", "exclude", "backup",
            "mechanism", "compatibility", "provenance"},
           {"schemaVersion", "id", "name", "revision", "identifiers", "mechanism", "provenance"});
    if (r.failed())
        return r.error();

    qint64 schemaVersion = 0;
    if (r.integer(json.value(QLatin1String("schemaVersion")), QStringLiteral("/schemaVersion"), schemaVersion, 1, kMaxInt)
        && schemaVersion != 1) {
        return Error{ErrorCode::Unsupported,
                     QStringLiteral("profile schemaVersion %1 is not supported by this version").arg(schemaVersion),
                     QStringLiteral("/schemaVersion")};
    }

    r.string(json.value(QLatin1String("id")), QStringLiteral("/id"), profile.id, 1, 100, &slugPattern());
    r.string(json.value(QLatin1String("name")), QStringLiteral("/name"), profile.name, 1, 200);
    qint64 revision = 0;
    if (r.integer(json.value(QLatin1String("revision")), QStringLiteral("/revision"), revision, 1,
                  std::numeric_limits<int>::max()))
        profile.revision = static_cast<int>(revision);

    readIdentifiers(r, json.value(QLatin1String("identifiers")), profile.identifiers);

    const bool hasSaves = json.contains(QLatin1String("saves"));
    const bool hasRegistry = json.contains(QLatin1String("registry"));
    if (!hasSaves && !hasRegistry)
        r.fail(QStringLiteral("/"), QStringLiteral("must have 'saves' or 'registry'"));
    if (hasSaves)
        readSaveRules(r, json.value(QLatin1String("saves")), profile.saves);
    if (hasRegistry)
        readRegistryRules(r, json.value(QLatin1String("registry")), profile.registry);
    if (json.contains(QLatin1String("exclude")))
        r.stringList(json.value(QLatin1String("exclude")), QStringLiteral("/exclude"), profile.exclude, 0, 64, 1, 256,
                     nullptr, false);
    if (json.contains(QLatin1String("backup")))
        readBackup(r, json.value(QLatin1String("backup")), profile.backup);
    readMechanism(r, json.value(QLatin1String("mechanism")), profile.mechanism);
    if (json.contains(QLatin1String("compatibility")))
        readCompatibility(r, json.value(QLatin1String("compatibility")), profile);
    readProvenance(r, json.value(QLatin1String("provenance")), profile.provenance);

    if (r.failed())
        return r.error();

    if (!supportedMechanisms().contains(profile.mechanism.type)) {
        return Error{ErrorCode::Unsupported,
                     QStringLiteral("mechanism '%1' requires a newer SaveForge version").arg(profile.mechanism.type),
                     QStringLiteral("/mechanism/type")};
    }

    checkSemantics(r, profile, rules);
    if (r.failed())
        return r.error();
    return profile;
}

Result<Profile> parseProfile(const QByteArray &json, const PathRules &rules)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return Error{ErrorCode::InvalidArgument, QStringLiteral("invalid JSON: ") + parseError.errorString(),
                     QStringLiteral("offset %1").arg(parseError.offset)};
    if (!doc.isObject())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("profile must be a JSON object"), QStringLiteral("/")};
    return parseProfile(doc.object(), rules);
}

} // namespace saveforge::profiles
