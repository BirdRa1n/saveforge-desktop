#pragma once

#include "saveforge/profiles/PlaceholderCatalog.h"

#include <QByteArray>
#include <QList>
#include <QStringList>
#include <QStringView>

#include <optional>

namespace saveforge::profiles {

// Outcome of a path-safety check: the violated rule IDs in check order
// ("P1".."P6" for save paths, "K1".."K3" for registry keys).
struct RuleCheck {
    QStringList violations;

    [[nodiscard]] bool valid() const { return violations.isEmpty(); }
};

// Path-safety rules from saveforge-specs/profile/path-rules.md.
//
// This is a port of the reference implementation (saveforge-specs
// scripts/lib/path-rules.mjs) and is tested against the same conformance
// vectors; any behavioral change must start in the specification.
class PathRules {
public:
    // Rules built from the catalog and denylist embedded in this build.
    [[nodiscard]] static const PathRules &builtin();

    // denylistJson: content of path-denylist.json.
    [[nodiscard]] static Result<PathRules> create(const PlaceholderCatalog &catalog, const QByteArray &denylistJson);

    [[nodiscard]] RuleCheck checkSavePath(QStringView path) const;
    [[nodiscard]] RuleCheck checkRegistryKey(QStringView key) const;

    // OS implied by the path's root placeholder; nullopt when OS-neutral or unknown.
    [[nodiscard]] std::optional<platform::Os> impliedOs(QStringView path) const;

    // Rule P6 applied to a list of segments (literal, glob or "**").
    // Also used at run time on concrete paths found on disk.
    [[nodiscard]] bool isSensitive(const QStringList &segments) const;

    [[nodiscard]] const PlaceholderCatalog &catalog() const { return m_catalog; }

private:
    PathRules() = default;

    PlaceholderCatalog m_catalog;
    QList<QStringList> m_denylist;
};

} // namespace saveforge::profiles
