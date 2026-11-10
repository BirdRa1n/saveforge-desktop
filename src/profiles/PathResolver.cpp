#include "savevault/profiles/PathResolver.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace savevault::profiles {

namespace {

const QStringList kSegmentPlaceholders{QStringLiteral("storeUserId"), QStringLiteral("storeGameId"),
                                       QStringLiteral("osUserName")};

// One real path segment and the index of the pattern segment that produced it
// (-1 for segments found while expanding a matched directory).
struct Step {
    QString name;
    qsizetype patternIndex = -1;
};

// An entry matched by the last pattern segment (a file or a directory).
struct Match {
    QFileInfo info;
    QList<Step> steps;
};

using Bindings = QHash<QString, QString>;

bool isLink(const QFileInfo &info)
{
    return info.isSymLink() || info.isJunction();
}

QFileInfoList listEntries(const QString &dir)
{
    QFileInfoList entries = QDir(dir).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                                    QDir::Name);
    entries.removeIf(isLink);
    return entries;
}

const QRegularExpression &placeholderTokenPattern()
{
    static const QRegularExpression re(QStringLiteral("<([^<>]+)>"));
    return re;
}

bool isGlob(const QString &segment)
{
    return segment.contains(u'*') || segment.contains(u'?');
}

bool segmentMatches(const QString &pattern, const QString &name, Qt::CaseSensitivity cs)
{
    if (!isGlob(pattern))
        return pattern.compare(name, cs) == 0;
    QString regex = QStringLiteral("^");
    for (QChar c : pattern) {
        if (c == u'*')
            regex += QStringLiteral(".*");
        else if (c == u'?')
            regex += u'.';
        else
            regex += QRegularExpression::escape(QString(c));
    }
    regex += u'$';
    const auto options = cs == Qt::CaseInsensitive ? QRegularExpression::CaseInsensitiveOption
                                                   : QRegularExpression::NoPatternOption;
    return QRegularExpression(regex, options).match(name).hasMatch();
}

// Glob over whole paths: '**' matches zero or more segments.
bool pathMatches(const QStringList &pattern, qsizetype pi, const QStringList &path, qsizetype si, Qt::CaseSensitivity cs)
{
    if (pi == pattern.size())
        return si == path.size();
    if (pattern.at(pi) == u"**") {
        for (qsizetype skip = si; skip <= path.size(); ++skip) {
            if (pathMatches(pattern, pi + 1, path, skip, cs))
                return true;
        }
        return false;
    }
    return si < path.size() && segmentMatches(pattern.at(pi), path.at(si), cs)
           && pathMatches(pattern, pi + 1, path, si + 1, cs);
}

bool isExcluded(const QStringList &exclude, const QList<Step> &steps, Qt::CaseSensitivity cs)
{
    if (exclude.isEmpty() || steps.isEmpty())
        return false;
    QStringList path;
    for (const Step &step : steps)
        path.append(step.name);
    for (const QString &pattern : exclude) {
        if (pattern.contains(u'/')) {
            if (pathMatches(pattern.split(u'/'), 0, path, 0, cs))
                return true;
        } else if (segmentMatches(pattern, path.last(), cs)) {
            return true;
        }
    }
    return false;
}

void walk(const QString &dir, const QStringList &pattern, qsizetype index, const QList<Step> &steps,
          Qt::CaseSensitivity cs, QList<Match> &out)
{
    const QString &segment = pattern.at(index);
    const bool last = index == pattern.size() - 1;

    if (segment == u"**") {
        if (last) {
            // Trailing "**": every entry below this directory is a match.
            for (const QFileInfo &entry : listEntries(dir))
                out.append({entry, steps + QList<Step>{{entry.fileName(), index}}});
            return;
        }
        walk(dir, pattern, index + 1, steps, cs, out);  // "**" as zero segments
        for (const QFileInfo &entry : listEntries(dir)) {
            if (entry.isDir())
                walk(entry.absoluteFilePath(), pattern, index, steps + QList<Step>{{entry.fileName(), index}}, cs, out);
        }
        return;
    }

    // Fast path for literal segments: one stat instead of a directory listing.
    // Most profile paths point to folders that do not exist on a given machine.
    // A miss is final unless a case-insensitive match is wanted on a
    // case-sensitive file system (e.g. a Proton prefix on Linux).
    if (!isGlob(segment)) {
        const QFileInfo direct(dir + u'/' + segment);
        const bool hostIsCaseInsensitive = platform::hostOs() != platform::Os::Linux;
        if (!direct.exists() && (cs == Qt::CaseSensitive || hostIsCaseInsensitive))
            return;
        if (direct.exists() && cs == Qt::CaseSensitive) {
            if (isLink(direct))
                return;
            const QList<Step> next = steps + QList<Step>{{segment, index}};
            if (last)
                out.append({direct, next});
            else if (direct.isDir())
                walk(direct.absoluteFilePath(), pattern, index + 1, next, cs, out);
            return;
        }
        // Otherwise list the directory to learn the real on-disk spelling.
    }

    for (const QFileInfo &entry : listEntries(dir)) {
        if (!segmentMatches(segment, entry.fileName(), cs))
            continue;
        const QList<Step> next = steps + QList<Step>{{entry.fileName(), index}};
        if (last)
            out.append({entry, next});
        else if (entry.isDir())
            walk(entry.absoluteFilePath(), pattern, index + 1, next, cs, out);
    }
}

// Expands a matched directory into all files below it.
void collectFiles(const QFileInfo &info, const QList<Step> &steps, QList<Match> &out)
{
    if (info.isFile()) {
        out.append({info, steps});
        return;
    }
    if (!info.isDir())
        return;
    for (const QFileInfo &entry : listEntries(info.absoluteFilePath()))
        collectFiles(entry, steps + QList<Step>{{entry.fileName(), -1}}, out);
}

// All combinations of segment placeholder values used by `segments`.
// Returns an empty list when a used placeholder has no value on this machine.
QList<Bindings> bindingCombinations(const QStringList &segments, const ResolveContext &context)
{
    QList<Bindings> combinations{Bindings{}};
    for (const QString &name : kSegmentPlaceholders) {
        const QString token = u'<' + name + u'>';
        if (!std::any_of(segments.begin(), segments.end(), [&](const QString &s) { return s.contains(token); }))
            continue;

        QStringList values;
        if (name == u"storeUserId")
            values = context.storeUserIds;
        else if (name == u"storeGameId" && !context.storeGameId.isEmpty())
            values = {context.storeGameId};
        else if (name == u"osUserName" && !context.osUserName.isEmpty())
            values = {context.osUserName};
        if (values.isEmpty())
            return {};

        QList<Bindings> expanded;
        for (const Bindings &combination : std::as_const(combinations)) {
            for (const QString &value : values) {
                Bindings next = combination;
                next.insert(name, value);
                expanded.append(next);
            }
        }
        combinations = expanded;
    }
    return combinations;
}

QString substitute(QString segment, const Bindings &bindings)
{
    for (auto it = bindings.cbegin(); it != bindings.cend(); ++it)
        segment.replace(u'<' + it.key() + u'>', it.value());
    return segment;
}

// Rebuilds the snapshot form of a path: segments that came from a pattern
// segment containing placeholders get the bound value replaced by the token.
QString templatePathFor(const QString &rootToken, const QStringList &patternSegments, const QList<Step> &steps,
                        const Bindings &bindings)
{
    QString result = rootToken;
    for (const Step &step : steps) {
        QString name = step.name;
        if (step.patternIndex >= 0) {
            const QString &original = patternSegments.at(step.patternIndex);
            for (auto it = bindings.cbegin(); it != bindings.cend(); ++it) {
                const QString token = u'<' + it.key() + u'>';
                if (!original.contains(token))
                    continue;
                const qsizetype at = name.indexOf(it.value());
                if (at >= 0)
                    name.replace(at, it.value().size(), token);
            }
        }
        result += u'/' + name;
    }
    return result;
}

void keepNewest(QList<Match> &matches, const QJsonObject &params)
{
    const qsizetype n = params.value(QLatin1String("n")).toInteger(1);
    const bool byName = params.value(QLatin1String("sortBy")).toString() == u"name";
    std::sort(matches.begin(), matches.end(), [byName](const Match &a, const Match &b) {
        if (byName)
            return a.info.fileName() > b.info.fileName();
        return a.info.lastModified() > b.info.lastModified();
    });
    if (matches.size() > n)
        matches.resize(n);
}

} // namespace

Result<QString> resolveRestoreTarget(const QString &templatePath, const QHash<QString, QString> &bindings,
                                     const ResolveContext &context, const PathRules &rules)
{
    QStringList violations = rules.checkSavePath(templatePath).violations;
    violations.removeIf([](const QString &rule) { return rule == u"P5" || rule == u"P6"; });
    if (!violations.isEmpty() || isGlob(templatePath))
        return Error{ErrorCode::InvalidArgument, QStringLiteral("not a concrete, safe path template"), templatePath};

    const QStringList segments = templatePath.split(u'/');
    const QString rootToken = segments.first();
    const QString rootDir = context.folders.value(rootToken.mid(1, rootToken.size() - 2));
    if (rootDir.isEmpty())
        return Error{ErrorCode::NotFound, QStringLiteral("%1 is not available on this machine").arg(rootToken),
                     templatePath};

    QStringList concrete;
    for (const QString &segment : segments.mid(1)) {
        QString value = segment;
        auto it = placeholderTokenPattern().globalMatch(segment);
        while (it.hasNext()) {
            const auto match = it.next();
            const QString bound = bindings.value(match.captured(1));
            if (bound.isEmpty())
                return Error{ErrorCode::InvalidArgument, QStringLiteral("no value for %1").arg(match.captured(0)),
                             templatePath};
            value.replace(match.captured(0), bound);
        }
        // Binding values come from backups and must not smuggle in separators or traversal.
        if (value.isEmpty() || value == u"." || value == u".." || value.contains(u'/') || value.contains(u'\\'))
            return Error{ErrorCode::InvalidArgument, QStringLiteral("unsafe path segment"), templatePath};
        concrete.append(value);
    }
    if (rules.isSensitive(concrete))
        return Error{ErrorCode::PermissionDenied, QStringLiteral("refusing to write to a sensitive location"),
                     templatePath};

    QString current = rootDir;
    for (const QString &segment : std::as_const(concrete)) {
        QString name = segment;
        // Look up the real spelling even when exists() already says yes: on a
        // case-insensitive file system it would succeed with any spelling.
        if (context.caseInsensitive && QFileInfo(current).isDir()) {
            for (const QFileInfo &entry : listEntries(current)) {
                if (entry.fileName().compare(name, Qt::CaseInsensitive) == 0) {
                    name = entry.fileName();
                    break;
                }
            }
        }
        current += u'/' + name;
        const QFileInfo info(current);
        if (isLink(info))
            return Error{ErrorCode::PermissionDenied, QStringLiteral("refusing to restore through a link"), current};
    }
    return current;
}

PathResolver::PathResolver(const PathRules &rules)
    : m_rules(rules)
{
}

bool PathResolver::ruleApplies(const SaveRule &rule, const QString &path, const ResolveContext &context) const
{
    if (rule.when.os && *rule.when.os != context.gameOs)
        return false;
    if (const auto implied = m_rules.impliedOs(path); implied && *implied != context.gameOs)
        return false;
    if (!rule.when.store.isEmpty() && rule.when.store != context.store)
        return false;
    return true;
}

QList<ResolvedFile> PathResolver::resolve(const Profile &profile, const ResolveContext &context) const
{
    const Qt::CaseSensitivity cs = context.caseInsensitive ? Qt::CaseInsensitive : Qt::CaseSensitive;
    const bool latestN = profile.mechanism.type == u"latest-n";

    QHash<QString, ResolvedFile> byPath;
    for (const SaveRule &rule : profile.saves) {
        for (const QString &path : rule.paths) {
            if (!ruleApplies(rule, path, context) || !m_rules.checkSavePath(path).valid())
                continue;

            const QStringList segments = path.split(u'/');
            const QString rootToken = segments.first();
            const QString rootDir = context.folders.value(rootToken.mid(1, rootToken.size() - 2));
            if (rootDir.isEmpty() || !QFileInfo(rootDir).isDir())
                continue;
            const QStringList patternSegments = segments.mid(1);

            for (const Bindings &bindings : bindingCombinations(patternSegments, context)) {
                QStringList concrete;
                for (const QString &segment : patternSegments)
                    concrete.append(substitute(segment, bindings));

                QList<Match> matches;
                walk(rootDir, concrete, 0, {}, cs, matches);
                if (latestN)
                    keepNewest(matches, profile.mechanism.params);

                QList<Match> files;
                for (const Match &match : std::as_const(matches))
                    collectFiles(match.info, match.steps, files);

                for (const Match &file : std::as_const(files)) {
                    const QString absolute = file.info.absoluteFilePath();
                    if (byPath.contains(absolute))
                        continue;
                    QStringList realSegments;
                    for (const Step &step : file.steps)
                        realSegments.append(step.name);
                    if (m_rules.isSensitive(realSegments) || isExcluded(profile.exclude, file.steps, cs))
                        continue;
                    byPath.insert(absolute, ResolvedFile{absolute, templatePathFor(rootToken, patternSegments, file.steps, bindings),
                                                         rule.id, rule.tags, bindings});
                }
            }
        }
    }

    QList<ResolvedFile> result = byPath.values();
    std::sort(result.begin(), result.end(),
              [](const ResolvedFile &a, const ResolvedFile &b) { return a.absolutePath < b.absolutePath; });
    return result;
}

} // namespace savevault::profiles
