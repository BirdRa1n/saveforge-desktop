#include "saveforge/core/Uuid.h"

#include <QRandomGenerator>

#include <array>

namespace saveforge {

QUuid createUuidV7(const QDateTime &now)
{
    const auto millis = static_cast<quint64>(now.toMSecsSinceEpoch());

    std::array<quint8, 16> bytes{};
    for (int i = 0; i < 6; ++i)
        bytes[static_cast<size_t>(i)] = static_cast<quint8>(millis >> (8 * (5 - i)));

    std::array<quint32, 3> random{};
    QRandomGenerator::system()->fillRange(random.data(), random.size());
    for (size_t i = 6; i < 16; ++i)
        bytes[i] = static_cast<quint8>(random[(i - 6) / 4] >> (8 * ((i - 6) % 4)));

    bytes[6] = static_cast<quint8>((bytes[6] & 0x0F) | 0x70); // version 7
    bytes[8] = static_cast<quint8>((bytes[8] & 0x3F) | 0x80); // RFC 4122 variant

    QUuid::Id128Bytes id{};
    std::copy(bytes.begin(), bytes.end(), id.data);
    return QUuid(id);
}

QString uuidToString(const QUuid &uuid)
{
    return uuid.toString(QUuid::WithoutBraces).toLower();
}

} // namespace saveforge
