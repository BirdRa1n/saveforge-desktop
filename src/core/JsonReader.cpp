#include "savevault/core/JsonReader.h"

#include <cmath>

namespace savevault {

QString JsonReader::child(const QString &pointer, const QString &key)
{
    QString escaped = key;
    escaped.replace(u'~', QLatin1String("~0")).replace(u'/', QLatin1String("~1"));
    return pointer + u'/' + escaped;
}

QString JsonReader::child(const QString &pointer, qsizetype index)
{
    return pointer + u'/' + QString::number(index);
}

bool JsonReader::fail(const QString &pointer, const QString &message, ErrorCode code)
{
    if (!m_error)
        m_error = Error{code, message, pointer.isEmpty() ? QStringLiteral("/") : pointer};
    return false;
}

bool JsonReader::keys(const QJsonObject &object, const QString &pointer, std::initializer_list<const char *> allowed,
                      std::initializer_list<const char *> required)
{
    if (failed())
        return false;
    for (const QString &key : object.keys()) {
        bool known = false;
        for (const char *name : allowed)
            known = known || key == QLatin1String(name);
        if (!known)
            return fail(child(pointer, key), QStringLiteral("unknown property"));
    }
    for (const char *name : required) {
        if (!object.contains(QLatin1String(name)))
            return fail(pointer, QStringLiteral("missing required property '%1'").arg(QLatin1String(name)));
    }
    return true;
}

bool JsonReader::object(const QJsonValue &value, const QString &pointer, QJsonObject &out)
{
    if (failed())
        return false;
    if (!value.isObject())
        return fail(pointer, QStringLiteral("must be an object"));
    out = value.toObject();
    return true;
}

bool JsonReader::array(const QJsonValue &value, const QString &pointer, QJsonArray &out, qsizetype minItems,
                       qsizetype maxItems)
{
    if (failed())
        return false;
    if (!value.isArray())
        return fail(pointer, QStringLiteral("must be an array"));
    out = value.toArray();
    if (out.size() < minItems)
        return fail(pointer, QStringLiteral("must have at least %1 item(s)").arg(minItems));
    if (out.size() > maxItems)
        return fail(pointer, QStringLiteral("must have at most %1 item(s)").arg(maxItems));
    return true;
}

bool JsonReader::string(const QJsonValue &value, const QString &pointer, QString &out, qsizetype minLength,
                        qsizetype maxLength, const QRegularExpression *pattern)
{
    if (failed())
        return false;
    if (!value.isString())
        return fail(pointer, QStringLiteral("must be a string"));
    out = value.toString();
    if (out.size() < minLength || out.size() > maxLength)
        return fail(pointer, QStringLiteral("has an invalid length"));
    if (pattern && !pattern->match(out).hasMatch())
        return fail(pointer, QStringLiteral("does not match pattern %1").arg(pattern->pattern()));
    return true;
}

bool JsonReader::enumeration(const QJsonValue &value, const QString &pointer, QString &out, const QStringList &allowed)
{
    if (!string(value, pointer, out, 1, 64))
        return false;
    if (!allowed.contains(out))
        return fail(pointer, QStringLiteral("must be one of: ") + allowed.join(QLatin1String(", ")));
    return true;
}

bool JsonReader::integer(const QJsonValue &value, const QString &pointer, qint64 &out, qint64 minimum, qint64 maximum)
{
    if (failed())
        return false;
    const double number = value.toDouble();
    if (!value.isDouble() || std::trunc(number) != number)
        return fail(pointer, QStringLiteral("must be an integer"));
    out = value.toInteger();
    if (out < minimum || out > maximum)
        return fail(pointer, QStringLiteral("must be between %1 and %2").arg(minimum).arg(maximum));
    return true;
}

bool JsonReader::boolean(const QJsonValue &value, const QString &pointer, bool &out)
{
    if (failed())
        return false;
    if (!value.isBool())
        return fail(pointer, QStringLiteral("must be a boolean"));
    out = value.toBool();
    return true;
}

bool JsonReader::stringList(const QJsonValue &value, const QString &pointer, QStringList &out, qsizetype minItems,
                            qsizetype maxItems, qsizetype minLength, qsizetype maxLength,
                            const QRegularExpression *pattern, bool unique, const QStringList *allowed)
{
    QJsonArray items;
    if (!array(value, pointer, items, minItems, maxItems))
        return false;
    out.clear();
    for (qsizetype i = 0; i < items.size(); ++i) {
        QString item;
        const bool ok = allowed ? enumeration(items.at(i), child(pointer, i), item, *allowed)
                                : string(items.at(i), child(pointer, i), item, minLength, maxLength, pattern);
        if (!ok)
            return false;
        if (unique && out.contains(item))
            return fail(child(pointer, i), QStringLiteral("duplicate item"));
        out.append(item);
    }
    return true;
}

bool JsonReader::integerList(const QJsonValue &value, const QString &pointer, QList<qint64> &out, qint64 minimum)
{
    QJsonArray items;
    if (!array(value, pointer, items))
        return false;
    out.clear();
    for (qsizetype i = 0; i < items.size(); ++i) {
        qint64 item = 0;
        if (!integer(items.at(i), child(pointer, i), item, minimum))
            return false;
        if (out.contains(item))
            return fail(child(pointer, i), QStringLiteral("duplicate item"));
        out.append(item);
    }
    return true;
}

} // namespace savevault
