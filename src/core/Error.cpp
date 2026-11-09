#include "saveforge/core/Error.h"

namespace saveforge {

QString errorCodeName(ErrorCode code)
{
    switch (code) {
    case ErrorCode::Unknown: return QStringLiteral("unknown");
    case ErrorCode::InvalidArgument: return QStringLiteral("invalid_argument");
    case ErrorCode::NotFound: return QStringLiteral("not_found");
    case ErrorCode::Io: return QStringLiteral("io");
    case ErrorCode::Corrupted: return QStringLiteral("corrupted");
    case ErrorCode::Unsupported: return QStringLiteral("unsupported");
    case ErrorCode::PermissionDenied: return QStringLiteral("permission_denied");
    case ErrorCode::Cancelled: return QStringLiteral("cancelled");
    }
    return QStringLiteral("unknown");
}

QString Error::toString() const
{
    QString out = errorCodeName(code) + QStringLiteral(": ") + message;
    if (!context.isEmpty())
        out += QStringLiteral(" (") + context + QStringLiteral(")");
    return out;
}

} // namespace saveforge
