#pragma once

#include "savevault/core/Result.h"
#include "savevault/platform/Os.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringView>

#include <optional>

namespace savevault::profiles {

// One root placeholder from savevault-specs/profile/placeholders.json.
struct RootPlaceholder {
    enum class Scope { User, Game };

    QString name;                     // without angle brackets, e.g. "winAppData"
    Scope scope = Scope::User;
    std::optional<platform::Os> os;   // nullopt: valid on every OS
};

// Catalog of path placeholders defined by the profile specification.
class PlaceholderCatalog {
public:
    // The catalog shipped with this build (embedded from savevault-specs).
    [[nodiscard]] static const PlaceholderCatalog &builtin();

    // Parses the placeholders.json format.
    [[nodiscard]] static Result<PlaceholderCatalog> fromJson(const QByteArray &json);

    // nullptr when `name` is not a root placeholder.
    [[nodiscard]] const RootPlaceholder *root(QStringView name) const;
    [[nodiscard]] bool isSegment(QStringView name) const;

    [[nodiscard]] const QList<RootPlaceholder> &roots() const { return m_roots; }
    [[nodiscard]] const QStringList &segments() const { return m_segments; }

private:
    QList<RootPlaceholder> m_roots;
    QStringList m_segments;
};

} // namespace savevault::profiles
