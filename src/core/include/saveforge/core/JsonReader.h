#pragma once

#include "saveforge/core/Error.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <initializer_list>
#include <limits>
#include <optional>

namespace saveforge {

// Strict reader for documents defined by saveforge-specs.
//
// Each check validates one value against a schema constraint and writes it to
// an output parameter. The first failure is recorded with the JSON pointer of
// the offending value; afterwards every check is a no-op returning false, so
// callers can read a whole section and test failed() once.
class JsonReader {
public:
    static constexpr qsizetype kUnbounded = std::numeric_limits<qsizetype>::max();

    [[nodiscard]] bool failed() const { return m_error.has_value(); }
    [[nodiscard]] const Error &error() const { return *m_error; }

    bool fail(const QString &pointer, const QString &message, ErrorCode code = ErrorCode::InvalidArgument);

    // Rejects keys outside `allowed` and reports the first missing `required` key.
    bool keys(const QJsonObject &object, const QString &pointer, std::initializer_list<const char *> allowed,
              std::initializer_list<const char *> required = {});

    bool object(const QJsonValue &value, const QString &pointer, QJsonObject &out);
    bool array(const QJsonValue &value, const QString &pointer, QJsonArray &out, qsizetype minItems = 0,
               qsizetype maxItems = kUnbounded);
    bool string(const QJsonValue &value, const QString &pointer, QString &out, qsizetype minLength = 0,
                qsizetype maxLength = kUnbounded, const QRegularExpression *pattern = nullptr);
    bool enumeration(const QJsonValue &value, const QString &pointer, QString &out, const QStringList &allowed);
    bool integer(const QJsonValue &value, const QString &pointer, qint64 &out,
                 qint64 minimum = std::numeric_limits<qint64>::min(),
                 qint64 maximum = std::numeric_limits<qint64>::max());
    bool boolean(const QJsonValue &value, const QString &pointer, bool &out);
    bool stringList(const QJsonValue &value, const QString &pointer, QStringList &out, qsizetype minItems,
                    qsizetype maxItems, qsizetype minLength, qsizetype maxLength,
                    const QRegularExpression *pattern = nullptr, bool unique = true,
                    const QStringList *allowed = nullptr);
    bool integerList(const QJsonValue &value, const QString &pointer, QList<qint64> &out, qint64 minimum = 1);

    // JSON pointer helpers (RFC 6901 escaping).
    [[nodiscard]] static QString child(const QString &pointer, const QString &key);
    [[nodiscard]] static QString child(const QString &pointer, qsizetype index);

private:
    std::optional<Error> m_error;
};

} // namespace saveforge
