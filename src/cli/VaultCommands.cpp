// `backup`, `snapshots` and `restore` commands.

#include "CliSupport.h"
#include "Commands.h"

#include "savevault/core/Uuid.h"
#include "savevault/detection/GameMatcher.h"
#include "savevault/detection/Launchers.h"
#include "savevault/profiles/ProfileParser.h"
#include "savevault/services/BackupService.h"
#include "savevault/services/RestoreService.h"

#include <QCommandLineParser>
#include <QFile>
#include <QSysInfo>

namespace savevault::cli {

namespace {

const QCommandLineOption kVault(QStringLiteral("vault"), QStringLiteral("Vault directory."), QStringLiteral("dir"));
const QCommandLineOption kProfile(QStringLiteral("profile"), QStringLiteral("Profile JSON file."), QStringLiteral("file"));
const QCommandLineOption kGame(QStringLiteral("game"), QStringLiteral("Profile id from the registry cache."), QStringLiteral("id"));
const QCommandLineOption kMap(QStringLiteral("map"),
                              QStringLiteral("Override a root folder, e.g. winAppData=D:/sandbox (repeatable)."),
                              QStringLiteral("placeholder=dir"));

// Stable per machine until the app stores its own id in settings: a UUIDv5 of
// the OS machine id, so the raw identifier never ends up in snapshots.
QUuid machineId()
{
    static const QUuid ns = QUuid::fromString(QStringLiteral("5b1c7d2e-8f4a-4c3b-9e6d-0a1b2c3d4e5f"));
    return QUuid::createUuidV5(ns, QSysInfo::machineUniqueId() + QSysInfo::machineHostName().toUtf8());
}

services::ServiceEnvironment serviceEnvironment()
{
    services::ServiceEnvironment env;
    env.machineId = machineId();
    env.appVersion = appVersion();
    return env;
}

bool parse(QCommandLineParser &parser, const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    parser.addHelpOption();
    if (!parser.parse(arguments)) {
        err << parser.errorText() << Qt::endl;
        return false;
    }
    if (parser.isSet(QStringLiteral("help"))) {
        out << parser.helpText();
        return false;
    }
    return true;
}

std::optional<vault::Vault> openVault(const QCommandLineParser &parser, QTextStream &err)
{
    if (!parser.isSet(kVault)) {
        err << "--vault is required" << Qt::endl;
        return std::nullopt;
    }
    auto vault = vault::Vault::open(parser.value(kVault));
    if (!vault.ok()) {
        err << "cannot open vault: " << vault.error().toString() << Qt::endl;
        return std::nullopt;
    }
    return std::move(vault).value();
}

// Profile + where it resolves: the matching installation if one is detected,
// otherwise the host user's folders; --map overrides individual roots.
std::optional<services::GameTarget> loadTarget(const QCommandLineParser &parser, QTextStream &out, QTextStream &err)
{
    services::GameTarget target;
    if (parser.isSet(kGame)) {
        // A profile from the local registry cache.
        const CatalogLoad load = loadCatalog();
        printWarnings(load.warnings, err);
        const auto entry = load.catalog.find(parser.value(kGame));
        if (!entry) {
            err << "no profile with id '" << parser.value(kGame) << "' in the registry cache" << Qt::endl;
            return std::nullopt;
        }
        target.profile = entry->profile;
        target.channel = entry->channel;
    } else if (parser.isSet(kProfile)) {
        QFile file(parser.value(kProfile));
        if (!file.open(QIODevice::ReadOnly)) {
            err << "cannot read profile: " << file.errorString() << Qt::endl;
            return std::nullopt;
        }
        auto profile = profiles::parseProfile(file.readAll());
        if (!profile.ok()) {
            err << "invalid profile: " << profile.error().toString() << Qt::endl;
            return std::nullopt;
        }
        target.profile = std::move(profile).value();
        target.channel = target.profile.provenance.source == u"local" ? QStringLiteral("local") : QStringLiteral("official");
    } else {
        err << "--game <id> or --profile <file> is required" << Qt::endl;
        return std::nullopt;
    }

    const detection::DetectionEnvironment env = detection::DetectionEnvironment::host();
    const QList<detection::InstallCandidate> installs = detection::detectInstalledGames(env);
    const detection::GameMatcher matcher({target.profile});
    QStringList storeUserIds;
    for (const detection::InstallCandidate &install : installs) {
        storeUserIds += install.storeUserIds;
        if (matcher.match(install)) {
            target.context = detection::resolveContextFor(install, env);
            out << "Using installation: [" << install.store << "] " << install.installDir << Qt::endl;
            break;
        }
    }
    if (target.context.folders.placeholders().isEmpty()) {
        storeUserIds.removeDuplicates();
        target.context = detection::hostResolveContext(env, storeUserIds);
        out << "No installation detected; using this user's folders." << Qt::endl;
    }

    for (const QString &mapping : parser.values(kMap)) {
        const QString name = mapping.section(u'=', 0, 0);
        const QString dir = mapping.section(u'=', 1);
        if (name.isEmpty() || dir.isEmpty()) {
            err << "invalid --map value: " << mapping << Qt::endl;
            return std::nullopt;
        }
        target.context.folders.set(name, dir);
    }
    return target;
}

QString actionLabel(services::FileAction action)
{
    switch (action) {
    case services::FileAction::Create: return QStringLiteral("create   ");
    case services::FileAction::Overwrite: return QStringLiteral("overwrite");
    case services::FileAction::Unchanged: return QStringLiteral("unchanged");
    case services::FileAction::Skip: return QStringLiteral("skip     ");
    }
    return {};
}

QString kindLabel(vault::SnapshotKind kind)
{
    switch (kind) {
    case vault::SnapshotKind::Manual: return QStringLiteral("manual");
    case vault::SnapshotKind::Auto: return QStringLiteral("auto");
    case vault::SnapshotKind::PreRestore: return QStringLiteral("pre-restore");
    case vault::SnapshotKind::Import: return QStringLiteral("import");
    }
    return {};
}

} // namespace

int runBackup(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Back up a game's saves into a vault."));
    const QCommandLineOption label(QStringLiteral("label"), QStringLiteral("Name for the snapshot."), QStringLiteral("text"));
    parser.addOptions({kVault, kGame, kProfile, kMap, label});
    if (!parse(parser, arguments, out, err))
        return parser.isSet(QStringLiteral("help")) ? 0 : 2;

    auto vault = openVault(parser, err);
    auto target = loadTarget(parser, out, err);
    if (!vault || !target)
        return 2;

    services::BackupOptions options;
    options.label = parser.value(label);
    options.onFileRead = [&out](const QString &path, qsizetype index, qsizetype total) {
        out << "  [" << index + 1 << '/' << total << "] " << path << Qt::endl;
        return true;
    };
    const auto snapshot = services::BackupService(*vault, serviceEnvironment()).backup(*target, options);
    if (!snapshot.ok()) {
        err << "backup failed: " << snapshot.error().toString() << Qt::endl;
        return 1;
    }
    out << "Snapshot " << uuidToString(snapshot.value().snapshotId) << ": " << snapshot.value().files.size()
        << " file(s)" << (snapshot.value().consistency == vault::Consistency::Inconsistent ? " (inconsistent)" : "")
        << Qt::endl;
    return 0;
}

int runSnapshots(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("List the snapshots in a vault."));
    const QCommandLineOption game(QStringLiteral("game"), QStringLiteral("Only this game id."), QStringLiteral("id"));
    parser.addOptions({kVault, game});
    if (!parse(parser, arguments, out, err))
        return parser.isSet(QStringLiteral("help")) ? 0 : 2;
    auto vault = openVault(parser, err);
    if (!vault)
        return 2;

    const QStringList games = parser.isSet(game) ? QStringList{parser.value(game)} : vault->games();
    for (const QString &gameId : games) {
        out << gameId << Qt::endl;
        for (const vault::Snapshot &s : vault->snapshots(gameId)) {
            out << "  " << uuidToString(s.snapshotId) << "  " << s.createdAt.toLocalTime().toString(Qt::ISODate) << "  "
                << kindLabel(s.kind) << "  " << s.files.size() << " file(s)";
            if (!s.label.isEmpty())
                out << "  \"" << s.label << '"';
            out << Qt::endl;
        }
    }
    return 0;
}

int runRestore(const QStringList &arguments, QTextStream &out, QTextStream &err)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Show a restore plan and, with --yes, restore a snapshot."));
    const QCommandLineOption snapshot(QStringLiteral("snapshot"), QStringLiteral("Snapshot id."), QStringLiteral("id"));
    const QCommandLineOption cloud(QStringLiteral("cloud"),
                                   QStringLiteral("Launcher-cloud conflicts: prefer-savevault or prefer-launcher."),
                                   QStringLiteral("policy"));
    const QCommandLineOption yes(QStringLiteral("yes"), QStringLiteral("Write the files (otherwise only plan)."));
    parser.addOptions({kVault, kGame, kProfile, kMap, snapshot, cloud, yes});
    if (!parse(parser, arguments, out, err))
        return parser.isSet(QStringLiteral("help")) ? 0 : 2;

    auto vault = openVault(parser, err);
    auto target = loadTarget(parser, out, err);
    const QUuid snapshotId = QUuid::fromString(parser.value(snapshot));
    if (!vault || !target)
        return 2;
    if (snapshotId.isNull()) {
        err << "--snapshot <id> is required" << Qt::endl;
        return 2;
    }

    services::RestoreService service(*vault, serviceEnvironment());
    const auto plan = service.plan(*target, snapshotId);
    if (!plan.ok()) {
        err << "cannot plan restore: " << plan.error().toString() << Qt::endl;
        return 1;
    }
    for (const services::PlannedFile &file : plan.value().files) {
        out << "  " << actionLabel(file.action) << "  "
            << (file.action == services::FileAction::Skip ? file.source.path + QStringLiteral(" (") + file.skipReason + u')'
                                                          : file.target)
            << (file.currentIsNewer ? "  [current file is newer]" : "") << Qt::endl;
    }
    for (const QString &extra : plan.value().extraFiles)
        out << "  keep       " << extra << "  [not in snapshot]" << Qt::endl;
    if (plan.value().gameRunning)
        out << "The game is running: close it before restoring." << Qt::endl;
    if (plan.value().launcherCloudConflict)
        out << "The launcher's cloud also syncs these saves (choose with --cloud)." << Qt::endl;

    if (!parser.isSet(yes)) {
        out << "Plan only; run again with --yes to restore." << Qt::endl;
        return 0;
    }
    services::CloudPolicy policy = services::CloudPolicy::Ask;
    if (parser.value(cloud) == u"prefer-savevault")
        policy = services::CloudPolicy::PreferSaveVault;
    else if (parser.value(cloud) == u"prefer-launcher")
        policy = services::CloudPolicy::PreferLauncher;

    const auto report = service.execute(*target, plan.value(), policy);
    if (!report.ok()) {
        err << "restore failed: " << report.error().toString() << Qt::endl;
        return 1;
    }
    out << "Restored " << report.value().written << " file(s)";
    if (report.value().preRestoreSnapshotId)
        out << "; previous files saved in snapshot " << uuidToString(*report.value().preRestoreSnapshotId);
    out << Qt::endl;
    return 0;
}

} // namespace savevault::cli
