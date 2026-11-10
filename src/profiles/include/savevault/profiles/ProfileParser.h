#pragma once

#include "savevault/core/Result.h"
#include "savevault/profiles/PathRules.h"
#include "savevault/profiles/Profile.h"

#include <QByteArray>
#include <QJsonObject>

namespace savevault::profiles {

// Mechanism types implemented by this build. Profiles using any other type
// are rejected with ErrorCode::Unsupported (spec: "skip and ask to update").
[[nodiscard]] const QStringList &supportedMechanisms();

// Parses and validates a profile v1 document.
//
// Validation covers every constraint of profile.v1.schema.json plus the
// semantic rules JSON Schema cannot express (path safety, OS coherence).
// Errors:
//   - InvalidArgument: the document violates the schema or a safety rule;
//     Error::context holds the JSON pointer of the offending value.
//   - Unsupported: unknown schemaVersion or mechanism type.
[[nodiscard]] Result<Profile> parseProfile(const QJsonObject &json,
                                           const PathRules &rules = PathRules::builtin());

[[nodiscard]] Result<Profile> parseProfile(const QByteArray &json,
                                           const PathRules &rules = PathRules::builtin());

} // namespace savevault::profiles
