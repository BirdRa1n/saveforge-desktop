#include "saveforge/profiles/PlaceholderCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace saveforge::profiles {

namespace {

Error catalogError(const QString &message)
{
    return Error{ErrorCode::Corrupted, QStringLiteral("invalid placeholder catalog: ") + message, {}};
}

} // namespace

Result<PlaceholderCatalog> PlaceholderCatalog::fromJson(const QByteArray &json)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return catalogError(parseError.errorString());

    PlaceholderCatalog catalog;
    const QJsonObject object = doc.object();

    for (const QJsonValue &value : object.value(QLatin1String("root")).toArray()) {
        const QJsonObject entry = value.toObject();
        RootPlaceholder placeholder;
        placeholder.name = entry.value(QLatin1String("name")).toString();
        const QString scope = entry.value(QLatin1String("scope")).toString();
        if (placeholder.name.isEmpty() || (scope != u"user" && scope != u"game"))
            return catalogError(QStringLiteral("bad root entry"));
        placeholder.scope = scope == u"game" ? RootPlaceholder::Scope::Game : RootPlaceholder::Scope::User;

        const QJsonValue os = entry.value(QLatin1String("os"));
        if (!os.isNull()) {
            placeholder.os = platform::osFromName(os.toString());
            if (!placeholder.os)
                return catalogError(QStringLiteral("unknown os for ") + placeholder.name);
        }
        catalog.m_roots.append(placeholder);
    }

    for (const QJsonValue &value : object.value(QLatin1String("segment")).toArray()) {
        const QString name = value.toObject().value(QLatin1String("name")).toString();
        if (name.isEmpty())
            return catalogError(QStringLiteral("bad segment entry"));
        catalog.m_segments.append(name);
    }

    if (catalog.m_roots.isEmpty())
        return catalogError(QStringLiteral("no root placeholders"));
    return catalog;
}

const PlaceholderCatalog &PlaceholderCatalog::builtin()
{
    static const PlaceholderCatalog catalog = [] {
        QFile file(QStringLiteral(":/saveforge/specs/profile/placeholders.json"));
        const bool opened = file.open(QIODevice::ReadOnly);
        Q_ASSERT_X(opened, "PlaceholderCatalog", "embedded placeholders.json missing");
        auto result = fromJson(opened ? file.readAll() : QByteArray());
        Q_ASSERT_X(result.ok(), "PlaceholderCatalog", "embedded placeholders.json invalid");
        return result.ok() ? std::move(result).value() : PlaceholderCatalog{};
    }();
    return catalog;
}

const RootPlaceholder *PlaceholderCatalog::root(QStringView name) const
{
    for (const RootPlaceholder &placeholder : m_roots) {
        if (placeholder.name == name)
            return &placeholder;
    }
    return nullptr;
}

bool PlaceholderCatalog::isSegment(QStringView name) const
{
    for (const QString &segment : m_segments) {
        if (segment == name)
            return true;
    }
    return false;
}

} // namespace saveforge::profiles
