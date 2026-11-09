// `registry import` and `registry status`.

#include "CliSupport.h"
#include "Commands.h"

#include "saveforge/registry/RegistryCache.h"

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>

namespace saveforge::cli {

namespace {

int runImport(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Verify a registry distribution and store it in the local cache."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("distribution"), QStringLiteral("Directory with root.json and channels/."));
    const QCommandLineOption channelOption(QStringLiteral("channel"), QStringLiteral("Only this channel (repeatable)."),
                                           QStringLiteral("name"));
    parser.addOption(channelOption);
    if (!parser.parse(arguments) || parser.positionalArguments().size() != 1) {
        err << (parser.errorText().isEmpty() ? parser.helpText() : parser.errorText()) << Qt::endl;
        return 2;
    }

    const QString source = parser.positionalArguments().first();
    QStringList channels = parser.values(channelOption);
    if (channels.isEmpty())
        channels = QDir(source + QStringLiteral("/channels")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (channels.isEmpty()) {
        err << "no channels found in " << source << Qt::endl;
        return 1;
    }

    const auto anchor = registry::builtinTrustAnchor();
    if (!anchor.ok()) {
        err << anchor.error().toString() << Qt::endl;
        return 1;
    }
    if (registry::builtinTrustAnchorIsForTesting())
        printWarnings({QStringLiteral("this build trusts the PUBLIC registry test keys (development only)")}, err);

    registry::RegistryCache cache(registryCacheDirectory());
    int failures = 0;
    for (const QString &channel : std::as_const(channels)) {
        QElapsedTimer timer;
        timer.start();
        const auto imported = cache.importChannel(source, channel, anchor.value(), appVersion());
        if (!imported.ok()) {
            err << channel << ": " << imported.error().toString() << Qt::endl;
            ++failures;
            continue;
        }
        out << channel << ": sequence " << imported.value().index.sequence << ", "
            << imported.value().profiles.size() << " profile(s), " << imported.value().rejectedProfiles.size()
            << " rejected (" << timer.elapsed() << " ms)" << Qt::endl;
        for (const registry::Notice &notice : imported.value().notices)
            out << "  " << notice.license << ": " << notice.attribution << Qt::endl;
    }
    out << "Cache: " << cache.directory() << Qt::endl;
    return failures == 0 ? 0 : 1;
}

int runStatus(QTextStream &out, QTextStream &err)
{
    QElapsedTimer timer;
    timer.start();
    const CatalogLoad load = loadCatalog();
    printWarnings(load.warnings, err);
    const registry::RegistryCache cache(registryCacheDirectory());
    out << "Cache: " << cache.directory() << Qt::endl;
    for (const QString &channel : cache.channels())
        out << "  " << channel << ": last accepted sequence " << cache.lastSequence(channel) << Qt::endl;
    out << load.catalog.size() << " profile(s) available (loaded in " << timer.elapsed() << " ms)." << Qt::endl;
    for (const registry::Notice &notice : load.notices)
        out << "  " << notice.license << ": " << notice.attribution << Qt::endl;
    return 0;
}

} // namespace

int runRegistry(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    const QString sub = arguments.value(1);
    const QStringList rest = QStringList{arguments.value(0)} + arguments.mid(2);
    if (sub == u"import")
        return runImport(rest, out, err);
    if (sub == u"status")
        return runStatus(out, err);
    err << "Usage: saveforge-cli registry import <distribution-dir> [--channel <name>]\n"
           "       saveforge-cli registry status\n";
    return 2;
}

} // namespace saveforge::cli
