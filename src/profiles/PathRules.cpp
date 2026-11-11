#include "savevault/profiles/PathRules.h"

#include "Wildcard.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace savevault::profiles {

namespace {

constexpr qsizetype kMaxPathLength = 512;
constexpr qsizetype kMaxRegistryKeyLength = 256;

// The checks below scan characters directly instead of running regular
// expressions: they run for every path of every registry profile (tens of
// thousands at each scan), where regex matching dominated the cost.

// Visits the placeholder tokens of `text`, the same ones the regular
// expression <[^<>]*> finds (leftmost, non-overlapping), passing each name
// without its brackets. Returns true when a '<' or '>' is left outside a token.
template <typename Visitor>
bool scanPlaceholders(QStringView text, Visitor &&visit)
{
    bool stray = false;
    qsizetype i = 0;
    while (i < text.size()) {
        const QChar c = text.at(i);
        if (c == u'<') {
            qsizetype k = i + 1;
            while (k < text.size() && text.at(k) != u'<' && text.at(k) != u'>')
                ++k;
            if (k < text.size() && text.at(k) == u'>') {
                visit(text.mid(i + 1, k - i - 1));
                i = k + 1;
                continue;
            }
            stray = true;
        } else if (c == u'>') {
            stray = true;
        }
        ++i;
    }
    return stray;
}

bool isGlob(QStringView segment)
{
    return segment.contains(u'*') || segment.contains(u'?');
}

bool hasPlaceholder(QStringView segment)
{
    bool found = false;
    scanPlaceholders(segment, [&found](QStringView) { found = true; });
    return found;
}

// "<name>" covering the whole text (the regex ^<([^<>]+)>$): the name, or empty.
QStringView wholeToken(QStringView text)
{
    if (text.size() < 3 || text.front() != u'<' || text.back() != u'>')
        return {};
    const QStringView name = text.mid(1, text.size() - 2);
    return name.contains(u'<') || name.contains(u'>') ? QStringView() : name;
}

// %NAME% (the regex %[^%]+%): a Windows environment variable.
bool hasPercentVariable(QStringView text)
{
    qsizetype open = text.indexOf(u'%');
    while (open >= 0) {
        const qsizetype close = text.indexOf(u'%', open + 1);
        if (close < 0)
            return false;
        if (close > open + 1)
            return true;
        open = close; // "%%": the second '%' may open a variable
    }
    return false;
}

// $NAME or ${NAME} (the regex \$(\{|[A-Za-z_])): a shell variable.
bool hasDollarVariable(QStringView text)
{
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) != u'$')
            continue;
        const char16_t next = text.at(i + 1).unicode();
        if (next == u'{' || next == u'_' || (next >= u'A' && next <= u'Z') || (next >= u'a' && next <= u'z'))
            return true;
    }
    return false;
}

bool hasControlCharOrBackslash(QStringView text)
{
    return std::any_of(text.begin(), text.end(), [](QChar c) { return c == u'\\' || c.unicode() < 0x20; });
}

// A path segment classified once for sensitivity checks.
struct PreparedSegment {
    enum class Kind { DoubleStar, Placeholder, Glob, Literal } kind = Kind::Literal;
    QString text;
};

QList<PreparedSegment> prepare(const QStringList &segments)
{
    QList<PreparedSegment> prepared;
    prepared.reserve(segments.size());
    for (const QString &segment : segments) {
        PreparedSegment p;
        p.text = segment;
        if (segment == u"**")
            p.kind = PreparedSegment::Kind::DoubleStar;
        else if (hasPlaceholder(segment))
            p.kind = PreparedSegment::Kind::Placeholder;
        else if (isGlob(segment))
            p.kind = PreparedSegment::Kind::Glob;
        prepared.append(std::move(p));
    }
    return prepared;
}

bool segmentCanMatch(const PreparedSegment &segment, const QString &target)
{
    switch (segment.kind) {
    case PreparedSegment::Kind::DoubleStar: return true;
    case PreparedSegment::Kind::Placeholder: return false;
    case PreparedSegment::Kind::Glob: return wildcardMatch(segment.text, target, Qt::CaseInsensitive);
    case PreparedSegment::Kind::Literal: return segment.text.compare(target, Qt::CaseInsensitive) == 0;
    }
    return false;
}

// True when a contiguous window of `segments` can match `sequence` and at least
// one element of the window matches literally.
bool matchesSequence(const QList<PreparedSegment> &segments, const QStringList &sequence)
{
    for (qsizetype start = 0; start + sequence.size() <= segments.size(); ++start) {
        bool ok = true;
        bool literalHit = false;
        for (qsizetype i = 0; i < sequence.size(); ++i) {
            const PreparedSegment &segment = segments.at(start + i);
            if (!segmentCanMatch(segment, sequence.at(i))) {
                ok = false;
                break;
            }
            if (segment.kind == PreparedSegment::Kind::Literal)
                literalHit = true;
        }
        if (ok && literalHit)
            return true;
    }
    return false;
}

void addOnce(QStringList &list, const QString &rule)
{
    if (!list.contains(rule))
        list.append(rule);
}

} // namespace

Result<PathRules> PathRules::create(const PlaceholderCatalog &catalog, const QByteArray &denylistJson)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(denylistJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return Error{ErrorCode::Corrupted, QStringLiteral("invalid path denylist"), parseError.errorString()};

    PathRules rules;
    rules.m_catalog = catalog;
    for (const QJsonValue &entry : doc.object().value(QLatin1String("sequences")).toArray()) {
        QStringList sequence;
        for (const QJsonValue &segment : entry.toArray())
            sequence.append(segment.toString());
        if (sequence.isEmpty() || sequence.contains(QString()))
            return Error{ErrorCode::Corrupted, QStringLiteral("invalid path denylist entry"), {}};
        rules.m_denylist.append(sequence);
    }
    return rules;
}

const PathRules &PathRules::builtin()
{
    static const PathRules rules = [] {
        QFile file(QStringLiteral(":/savevault/specs/profile/path-denylist.json"));
        const bool opened = file.open(QIODevice::ReadOnly);
        Q_ASSERT_X(opened, "PathRules", "embedded path-denylist.json missing");
        auto result = create(PlaceholderCatalog::builtin(), opened ? file.readAll() : QByteArray());
        Q_ASSERT_X(result.ok(), "PathRules", "embedded path-denylist.json invalid");
        return result.ok() ? std::move(result).value() : PathRules{};
    }();
    return rules;
}

RuleCheck PathRules::checkSavePath(QStringView pathView) const
{
    RuleCheck check;
    const QString path = pathView.toString();

    // P1: syntax
    if (path.isEmpty() || path.size() > kMaxPathLength)
        return RuleCheck{{QStringLiteral("P1")}};
    if (hasControlCharOrBackslash(path) || path.endsWith(u'/') || path.contains(QLatin1String("//"))
        || path.startsWith(u'~') || hasPercentVariable(path) || hasDollarVariable(path)) {
        addOnce(check.violations, QStringLiteral("P1"));
    }

    const QStringList segments = path.split(u'/');
    const QString &first = segments.first();
    const QStringList rest = segments.mid(1);

    // P2: whole first segment is a known root placeholder
    const RootPlaceholder *root = nullptr;
    if (const QStringView name = wholeToken(first); !name.isEmpty())
        root = m_catalog.root(name);
    if (!root)
        addOnce(check.violations, QStringLiteral("P2"));

    // P3: only known segment placeholders after the root, no stray brackets
    for (const QString &segment : rest) {
        bool unknown = false;
        const bool stray = scanPlaceholders(segment, [this, &unknown](QStringView name) {
            if (!m_catalog.isSegment(name))
                unknown = true;
        });
        if (unknown || stray)
            addOnce(check.violations, QStringLiteral("P3"));
    }

    // P4: no relative segments
    if (std::any_of(rest.begin(), rest.end(), [](const QString &s) { return s == u"." || s == u".."; }))
        addOnce(check.violations, QStringLiteral("P4"));

    // P5: anchored below the root
    if (rest.isEmpty()) {
        addOnce(check.violations, QStringLiteral("P5"));
    } else if (root && root->scope == RootPlaceholder::Scope::User) {
        if (isGlob(rest.first()) || hasPlaceholder(rest.first()))
            addOnce(check.violations, QStringLiteral("P5"));
    } else if (std::all_of(rest.begin(), rest.end(), [](const QString &s) { return s == u"**"; })) {
        addOnce(check.violations, QStringLiteral("P5"));
    }

    // P6: sensitive locations
    if (isSensitive(rest))
        addOnce(check.violations, QStringLiteral("P6"));

    return check;
}

RuleCheck PathRules::checkRegistryKey(QStringView keyView) const
{
    RuleCheck check;
    const QString key = keyView.toString();

    if (key.isEmpty() || key.size() > kMaxRegistryKeyLength || hasControlCharOrBackslash(key)
        || key.contains(u'*') || key.contains(u'?') || key.endsWith(u'/') || key.contains(QLatin1String("//"))) {
        check.violations.append(QStringLiteral("K1"));
    }

    const QStringList segments = key.split(u'/');
    if (segments.size() < 3 || segments.at(0) != u"HKCU"
        || segments.at(1).compare(QLatin1String("software"), Qt::CaseInsensitive) != 0) {
        check.violations.append(QStringLiteral("K2"));
    }

    if (std::any_of(segments.begin(), segments.end(), [](const QString &s) { return s == u"." || s == u".."; }))
        check.violations.append(QStringLiteral("K3"));

    return check;
}

std::optional<platform::Os> PathRules::impliedOs(QStringView path) const
{
    // The leading "<name>" token (the regex ^<([^<>]+)>).
    if (!path.startsWith(u'<'))
        return std::nullopt;
    qsizetype close = 1;
    while (close < path.size() && path.at(close) != u'<' && path.at(close) != u'>')
        ++close;
    if (close >= path.size() || path.at(close) != u'>' || close == 1)
        return std::nullopt;
    const RootPlaceholder *root = m_catalog.root(path.mid(1, close - 1));
    return root ? root->os : std::nullopt;
}

bool PathRules::isSensitive(const QStringList &segments) const
{
    const QList<PreparedSegment> prepared = prepare(segments);
    return std::any_of(m_denylist.begin(), m_denylist.end(),
                       [&](const QStringList &sequence) { return matchesSequence(prepared, sequence); });
}

} // namespace savevault::profiles
