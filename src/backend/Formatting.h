#pragma once

#include "saveforge/detection/GameMatcher.h"
#include "saveforge/services/Library.h"
#include "saveforge/vault/Snapshot.h"

#include <QColor>
#include <QDateTime>
#include <QString>

namespace saveforge::backend {

// User-facing text for the QML interface. Strings are translatable (pt-BR is
// the source language).

// "1 jogo", "22.262 perfis": pt-BR number formatting plus singular/plural.
[[nodiscard]] QString countText(qint64 n, const QString &singular, const QString &plural);

// "agora", "há 5 min", "há 2 h", "ontem", "há 3 dias", or a date.
[[nodiscard]] QString relativeTime(const QDateTime &when, const QDateTime &now = QDateTime::currentDateTimeUtc());

// "Steam", "Epic Games", "GOG" (with "via Heroic" when relevant), or how the
// game was found when no launcher installed it.
[[nodiscard]] QString storeLabel(const services::LibraryGame &game);

// Filter key used by the library: "steam", "epic", "gog" or "other".
[[nodiscard]] QString storeKey(const services::LibraryGame &game);

[[nodiscard]] QString reasonLabel(detection::MatchReason reason, bool hasInstall);
[[nodiscard]] QString snapshotKindLabel(vault::SnapshotKind kind);

// Stable placeholder cover color for a game id, from a muted palette that
// keeps white initials readable on the Forja theme.
[[nodiscard]] QColor coverColor(const QString &gameId);

} // namespace saveforge::backend
