#pragma once

#include "saveforge/core/Result.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace saveforge::detection {

// A node of Valve's text KeyValues format (VDF/ACF), used by Steam for
// libraryfolders.vdf, appmanifest_*.acf and loginusers.vdf.
// A node is either a value (`"key" "value"`) or an object (`"key" { ... }`).
// Keys are compared case-insensitively, as Steam does.
struct VdfNode {
    QString key;
    QString value;
    QList<VdfNode> children;
    bool isObject = false;

    // First child with this key, or nullptr.
    [[nodiscard]] const VdfNode *child(QStringView name) const;
    // Value of the first child with this key, or `fallback`.
    [[nodiscard]] QString valueOf(QStringView name, const QString &fallback = {}) const;
};

// Parses text VDF into a synthetic root object whose children are the
// top-level entries. Supports quoted and unquoted tokens, escape sequences
// (\" \\ \n \t), // comments and platform conditionals ([$WIN32]), which are
// ignored. Input is limited in size and nesting depth.
[[nodiscard]] Result<VdfNode> parseVdf(const QByteArray &text);

} // namespace saveforge::detection
