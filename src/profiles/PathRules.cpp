#include "saveforge/profiles/PathRules.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace saveforge::profiles {

namespace {

constexpr qsizetype kMaxPathLength = 512;
constexpr qsizetype kMaxRegistryKeyLength = 256;

const QRegularExpression &placeholderToken()
{
    static const QRegularExpression re(QStringLiteral("<[^<>]*>"));
    return re;
}

bool isGlob(QStringView segment)
{
    return segment.contains(u'*') || segment.contains(u'?');
}

bool hasPlaceholder(const QString &segment)
{
    return placeholderToken().match(segment).hasMatch();
}

bool hasControlCharOrBackslash(QStringView text)
{
    return std::any_of(text.begin(), text.end(), [](QChar c) { return c == u'\\' || c.unicode() < 0x20; });
}

// Single-segment glob: '*' = any run within the segment, '?' = one character.
// '[' and ']' are literal (spec: profile/SPEC.md section 4.1).
QRegularExpression segmentGlobToRegex(QStringView glob)
{
    QString pattern = QStringLiteral("^");
    for (QChar c : glob) {
        if (c == u'*')
            pattern += QStringLiteral("[^/]*");
        else if (c == u'?')
            pattern += QStringLiteral("[^/]");
        else
            pattern += QRegularExpression::escape(QString(c));
    }
    pattern += u'$';
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
}

// A path segment prepared once for sensitivity checks: classifying it and
// compiling its glob is far more expensive than comparing it.
struct PreparedSegment {
    enum class Kind { DoubleStar, Placeholder, Glob, Literal } kind = Kind::Literal;
    QString text;
    QRegularExpression glob;
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
        else if (isGlob(segment)) {
            p.kind = PreparedSegment::Kind::Glob;
            p.glob = segmentGlobToRegex(segment);
        }
        prepared.append(std::move(p));
    }
    return prepared;
}

bool segmentCanMatch(const PreparedSegment &segment, const QString &target)
{
    switch (segment.kind) {
    case PreparedSegment::Kind::DoubleStar: return true;
    case PreparedSegment::Kind::Placeholder: return false;
    case PreparedSegment::Kind::Glob: return segment.glob.match(target).hasMatch();
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
        QFile file(QStringLiteral(":/saveforge/specs/profile/path-denylist.json"));
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
    static const QRegularExpression envPercent(QStringLiteral("%[^%]+%"));
    static const QRegularExpression envDollar(QStringLiteral("\\$(\\{|[A-Za-z_])"));
    if (hasControlCharOrBackslash(path) || path.endsWith(u'/') || path.contains(QLatin1String("//"))
        || path.startsWith(u'~') || envPercent.match(path).hasMatch() || envDollar.match(path).hasMatch()) {
        addOnce(check.violations, QStringLiteral("P1"));
    }

    const QStringList segments = path.split(u'/');
    const QString &first = segments.first();
    const QStringList rest = segments.mid(1);

    // P2: whole first segment is a known root placeholder
    const RootPlaceholder *root = nullptr;
    static const QRegularExpression rootToken(QStringLiteral("^<([^<>]+)>$"));
    if (const auto match = rootToken.match(first); match.hasMatch())
        root = m_catalog.root(match.capturedView(1));
    if (!root)
        addOnce(check.violations, QStringLiteral("P2"));

    // P3: only known segment placeholders after the root, no stray brackets
    for (const QString &segment : rest) {
        auto it = placeholderToken().globalMatch(segment);
        while (it.hasNext()) {
            const QString token = it.next().captured(0);
            if (!m_catalog.isSegment(QStringView(token).mid(1, token.size() - 2)))
                addOnce(check.violations, QStringLiteral("P3"));
        }
        QString stripped = segment;
        stripped.remove(placeholderToken());
        if (stripped.contains(u'<') || stripped.contains(u'>'))
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
    static const QRegularExpression rootToken(QStringLiteral("^<([^<>]+)>"));
    const auto match = rootToken.matchView(path);
    if (!match.hasMatch())
        return std::nullopt;
    const RootPlaceholder *root = m_catalog.root(match.capturedView(1));
    return root ? root->os : std::nullopt;
}

bool PathRules::isSensitive(const QStringList &segments) const
{
    const QList<PreparedSegment> prepared = prepare(segments);
    return std::any_of(m_denylist.begin(), m_denylist.end(),
                       [&](const QStringList &sequence) { return matchesSequence(prepared, sequence); });
}

} // namespace saveforge::profiles
