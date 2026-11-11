#pragma once

#include <QStringView>

namespace savevault::profiles {

// Matches one path segment against a glob: '*' is any run of characters and
// '?' exactly one character (a UTF-16 surrogate pair counts as one); every
// other character is literal, '[' and ']' included (profile/SPEC.md 4.1).
//
// Internal to the profiles module. It replaces compiling a regular expression
// per comparison, which dominated profile validation and the library scan.
[[nodiscard]] inline bool wildcardMatch(QStringView pattern, QStringView name, Qt::CaseSensitivity cs)
{
    const auto same = [cs](QChar a, QChar b) {
        return cs == Qt::CaseSensitive ? a == b : a.toCaseFolded() == b.toCaseFolded();
    };
    const auto charLength = [&name](qsizetype i) -> qsizetype {
        return name.at(i).isHighSurrogate() && i + 1 < name.size() && name.at(i + 1).isLowSurrogate() ? 2 : 1;
    };
    qsizetype p = 0;
    qsizetype n = 0;
    qsizetype star = -1;
    qsizetype resume = 0;
    while (n < name.size()) {
        if (p < pattern.size() && pattern.at(p) == u'?') {
            ++p;
            n += charLength(n);
        } else if (p < pattern.size() && pattern.at(p) != u'*' && same(pattern.at(p), name.at(n))) {
            ++p;
            ++n;
        } else if (p < pattern.size() && pattern.at(p) == u'*') {
            star = p++;
            resume = n;
        } else if (star >= 0) {
            // Let the last '*' absorb one more character and retry.
            p = star + 1;
            resume += charLength(resume);
            n = resume;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern.at(p) == u'*')
        ++p;
    return p == pattern.size();
}

} // namespace savevault::profiles
