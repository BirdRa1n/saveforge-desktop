#pragma once

#include <QDateTime>
#include <QUuid>

namespace savevault {

// RFC 9562 UUID version 7: 48-bit Unix timestamp in milliseconds followed by
// random bits. Identifiers sort by creation time, which keeps snapshot and
// export IDs naturally ordered. (QUuid::createUuidV7 only exists since Qt 6.9.)
[[nodiscard]] QUuid createUuidV7(const QDateTime &now = QDateTime::currentDateTimeUtc());

// Lower-case canonical text without braces, as used in every SaveVault document.
[[nodiscard]] QString uuidToString(const QUuid &uuid);

} // namespace savevault
