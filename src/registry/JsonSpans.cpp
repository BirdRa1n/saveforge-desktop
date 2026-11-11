#include "savevault/registry/JsonSpans.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace savevault::registry {

namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

} // namespace

JsonSpans::JsonSpans(const QByteArray &text)
    : m_text(text)
{
}

qsizetype JsonSpans::skipSpace(qsizetype i, qsizetype end) const
{
    while (i < end && isSpace(m_text.at(i)))
        ++i;
    return i;
}

qsizetype JsonSpans::stringEnd(qsizetype i, qsizetype end) const
{
    // m_text.at(i) is the opening quote.
    for (++i; i < end; ++i) {
        const char c = m_text.at(i);
        if (c == '\\')
            ++i; // the escaped character, whatever it is
        else if (c == '"')
            return i + 1;
    }
    return -1;
}

qsizetype JsonSpans::valueEnd(qsizetype i, qsizetype end) const
{
    if (i >= end)
        return -1;
    const char first = m_text.at(i);
    if (first == '"')
        return stringEnd(i, end);
    if (first == '{' || first == '[') {
        // Containers: track nesting; strings may hold brackets.
        QByteArray stack(1, first == '{' ? '}' : ']');
        for (++i; i < end; ++i) {
            const char c = m_text.at(i);
            if (c == '"') {
                i = stringEnd(i, end);
                if (i < 0)
                    return -1;
                --i;
            } else if (c == '{' || c == '[') {
                stack.append(c == '{' ? '}' : ']');
            } else if (c == '}' || c == ']') {
                if (stack.isEmpty() || stack.back() != c)
                    return -1;
                stack.chop(1);
                if (stack.isEmpty())
                    return i + 1;
            }
        }
        return -1;
    }
    // Number, true, false or null: up to the next delimiter.
    while (i < end && !isSpace(m_text.at(i)) && m_text.at(i) != ',' && m_text.at(i) != '}' && m_text.at(i) != ']')
        ++i;
    return i;
}

std::optional<JsonSpan> JsonSpans::root() const
{
    const qsizetype begin = skipSpace(0, m_text.size());
    const qsizetype end = valueEnd(begin, m_text.size());
    if (end < 0 || skipSpace(end, m_text.size()) != m_text.size())
        return std::nullopt;
    return JsonSpan{begin, end};
}

std::optional<QHash<QString, JsonSpan>> JsonSpans::objectMembers(JsonSpan span) const
{
    if (span.end - span.begin < 2 || m_text.at(span.begin) != '{' || m_text.at(span.end - 1) != '}')
        return std::nullopt;
    QHash<QString, JsonSpan> members;
    const qsizetype end = span.end - 1;
    qsizetype i = skipSpace(span.begin + 1, end);
    while (i < end) {
        if (m_text.at(i) != '"')
            return std::nullopt;
        const qsizetype keyEnd = stringEnd(i, end);
        if (keyEnd < 0)
            return std::nullopt;
        const auto key = parse({i, keyEnd});
        if (!key || !key->isString())
            return std::nullopt;
        i = skipSpace(keyEnd, end);
        if (i >= end || m_text.at(i) != ':')
            return std::nullopt;
        i = skipSpace(i + 1, end);
        const qsizetype valueStop = valueEnd(i, end);
        if (valueStop < 0 || valueStop == i)
            return std::nullopt;
        members.insert(key->toString(), {i, valueStop});
        i = skipSpace(valueStop, end);
        if (i < end) {
            if (m_text.at(i) != ',')
                return std::nullopt;
            i = skipSpace(i + 1, end);
            if (i >= end)
                return std::nullopt; // trailing comma
        }
    }
    return members;
}

std::optional<QList<JsonSpan>> JsonSpans::arrayElements(JsonSpan span) const
{
    if (span.end - span.begin < 2 || m_text.at(span.begin) != '[' || m_text.at(span.end - 1) != ']')
        return std::nullopt;
    QList<JsonSpan> elements;
    const qsizetype end = span.end - 1;
    qsizetype i = skipSpace(span.begin + 1, end);
    while (i < end) {
        const qsizetype valueStop = valueEnd(i, end);
        if (valueStop < 0 || valueStop == i)
            return std::nullopt;
        elements.append({i, valueStop});
        i = skipSpace(valueStop, end);
        if (i < end) {
            if (m_text.at(i) != ',')
                return std::nullopt;
            i = skipSpace(i + 1, end);
            if (i >= end)
                return std::nullopt; // trailing comma
        }
    }
    return elements;
}

std::optional<QJsonValue> JsonSpans::parse(JsonSpan span) const
{
    if (span.begin < 0 || span.end > m_text.size() || span.end <= span.begin)
        return std::nullopt;
    QJsonParseError error{};
    const char first = m_text.at(span.begin);
    if (first == '{' || first == '[') {
        // Containers parse in place, without copying the slice.
        const QJsonDocument document = QJsonDocument::fromJson(
            QByteArray::fromRawData(m_text.constData() + span.begin, span.end - span.begin), &error);
        if (error.error != QJsonParseError::NoError)
            return std::nullopt;
        return document.isObject() ? QJsonValue(document.object()) : QJsonValue(document.array());
    }
    // Scalars are wrapped in an array, the only form QJsonDocument parses.
    QByteArray wrapped;
    wrapped.reserve(span.end - span.begin + 2);
    wrapped.append('[');
    wrapped.append(m_text.constData() + span.begin, span.end - span.begin);
    wrapped.append(']');
    const QJsonDocument document = QJsonDocument::fromJson(wrapped, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray() || document.array().size() != 1)
        return std::nullopt;
    return document.array().first();
}

} // namespace savevault::registry
