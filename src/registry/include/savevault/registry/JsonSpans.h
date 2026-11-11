#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QList>
#include <QString>

#include <optional>

namespace savevault::registry {

// Byte range [begin, end) of one JSON value inside a larger text.
struct JsonSpan {
    qsizetype begin = 0;
    qsizetype end = 0;
};

// Locates values inside a JSON text without building a document for all of it.
//
// A registry bundle holds tens of thousands of profiles; QJsonDocument would
// keep a tree of about eight times the text in memory just to hand them out
// one by one. This scanner only finds where each value starts and ends
// (respecting strings and escapes), so a caller can parse one value at a time
// with QJsonDocument, which still validates every value it reads.
//
// Structural problems (unbalanced brackets, a missing ':' or ',', an
// unterminated string) make the functions return std::nullopt.
class JsonSpans {
public:
    // `text` must outlive this object.
    explicit JsonSpans(const QByteArray &text);

    // The whole text as one value (surrounding whitespace excluded).
    [[nodiscard]] std::optional<JsonSpan> root() const;
    // Members of the object at `span`: key -> value span. On a duplicated key
    // the last value wins, as in QJsonDocument.
    [[nodiscard]] std::optional<QHash<QString, JsonSpan>> objectMembers(JsonSpan span) const;
    // Elements of the array at `span`, in order.
    [[nodiscard]] std::optional<QList<JsonSpan>> arrayElements(JsonSpan span) const;
    // Parses one value with QJsonDocument; std::nullopt when it is not valid JSON.
    [[nodiscard]] std::optional<QJsonValue> parse(JsonSpan span) const;

private:
    [[nodiscard]] qsizetype skipSpace(qsizetype i, qsizetype end) const;
    // Index just after the value that starts at `i`, or -1.
    [[nodiscard]] qsizetype valueEnd(qsizetype i, qsizetype end) const;
    [[nodiscard]] qsizetype stringEnd(qsizetype i, qsizetype end) const;

    const QByteArray &m_text;
};

} // namespace savevault::registry
