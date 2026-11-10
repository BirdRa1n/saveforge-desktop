#include "savevault/services/Library.h"

#include "savevault/registry/RegistryCache.h"
#include "savevault/registry/RegistryUpdater.h"
#include "savevault/services/LocalProfiles.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace savevault::services {

CatalogLoad loadCatalogFromCache(const QString &registryCacheDir, const QString &appVersion,
                                 const QString &localProfilesDir)
{
    CatalogLoad result;
    // Local profiles do not depend on the registry and work even without a trust anchor.
    if (!localProfilesDir.isEmpty()) {
        LocalProfileStore::Load local = LocalProfileStore(localProfilesDir).load();
        for (const QString &warning : std::as_const(local.warnings))
            result.warnings.append(QStringLiteral("local profile skipped: ") + warning);
        result.catalog.addChannel(QStringLiteral("local"), local.profiles);
    }
    const auto anchor = registry::builtinTrustAnchor();
    if (!anchor.ok()) {
        result.warnings.append(QStringLiteral("registry disabled: ") + anchor.error().toString());
        return result;
    }
    result.usingTestKeys = registry::builtinTrustAnchorIsForTesting();
    if (result.usingTestKeys)
        result.warnings.append(QStringLiteral("this build trusts the PUBLIC registry test keys (development only)"));

    registry::RegistryCache cache(registryCacheDir);
    if (const QString bundled = registry::bundledDistribution(); !bundled.isEmpty()) {
        for (const registry::ChannelUpdate &seeded :
             registry::importDistribution(bundled, cache, anchor.value(), appVersion, {}, true)) {
            if (seeded.error)
                result.warnings.append(QStringLiteral("bundled channel %1 unusable: %2").arg(seeded.channel, seeded.error->toString()));
        }
    }
    for (const QString &channel : cache.channels()) {
        auto loaded = cache.loadChannel(channel, anchor.value(), appVersion);
        if (!loaded.ok()) {
            result.warnings.append(QStringLiteral("channel %1 unavailable: %2").arg(channel, loaded.error().toString()));
            continue;
        }
        if (loaded.value().expired)
            result.warnings.append(QStringLiteral("channel %1 is past its expiry date").arg(channel));
        result.catalog.addChannel(channel, loaded.value().profiles);
        result.notices += loaded.value().notices;
    }
    return result;
}

LibraryScan scanLibrary(const registry::ProfileCatalog &catalog, const QList<detection::InstallCandidate> &installs,
                        const detection::DetectionEnvironment &env, const vault::Vault *vault)
{
    LibraryScan scan;
    const QList<registry::CatalogEntry> entries = catalog.entries();
    QHash<QString, const registry::CatalogEntry *> byId;
    QList<profiles::Profile> profiles;
    profiles.reserve(entries.size());
    for (const registry::CatalogEntry &entry : entries) {
        byId.insert(entry.profile.id, &entry);
        profiles.append(entry.profile);
    }

    const detection::DetectionReport report = detection::detectGames(profiles, installs, env);
    scan.unmatched = report.unmatched;

    QStringList storeUserIds;
    for (const detection::InstallCandidate &install : installs)
        storeUserIds += install.storeUserIds;
    storeUserIds.removeDuplicates();
    const profiles::ResolveContext hostContext = detection::hostResolveContext(env, storeUserIds);

    QSet<QString> present;
    for (const detection::DetectedGame &detected : report.games) {
        if (present.contains(detected.profileId))
            continue; // the same game installed twice: keep the first (strongest) match
        const registry::CatalogEntry *entry = byId.value(detected.profileId);
        if (!entry)
            continue;
        LibraryGame game;
        game.target.profile = entry->profile;
        game.target.channel = entry->channel;
        game.target.context = detected.install ? detection::resolveContextFor(*detected.install, env) : hostContext;
        game.reason = detected.reason;
        game.install = detected.install;
        game.saveFilesFound = detected.saveFilesFound;
        const QStringList &cloud = entry->profile.backup.cloudSync;
        game.launcherCloud = detected.install ? cloud.contains(detected.install->store) : !cloud.isEmpty();
        scan.games.append(game);
        present.insert(detected.profileId);
    }

    if (vault) {
        // Games with backups that are no longer detected stay in the library.
        for (const QString &gameId : vault->games()) {
            if (present.contains(gameId))
                continue;
            const registry::CatalogEntry *entry = byId.value(gameId);
            if (!entry)
                continue; // profile no longer available; its snapshots remain in the vault
            LibraryGame game;
            game.target.profile = entry->profile;
            game.target.channel = entry->channel;
            game.target.context = hostContext;
            game.launcherCloud = !entry->profile.backup.cloudSync.isEmpty();
            scan.games.append(game);
            present.insert(gameId);
        }
        for (LibraryGame &game : scan.games) {
            const QList<vault::Snapshot> snapshots = vault->snapshots(game.id());
            game.snapshotCount = snapshots.size();
            for (auto it = snapshots.crbegin(); it != snapshots.crend(); ++it) {
                if (it->kind != vault::SnapshotKind::PreRestore) { // undo points are not "backups"
                    game.lastSnapshot = *it;
                    break;
                }
            }
        }
    }

    std::sort(scan.games.begin(), scan.games.end(), [](const LibraryGame &a, const LibraryGame &b) {
        return a.target.profile.name.compare(b.target.profile.name, Qt::CaseInsensitive) < 0;
    });
    return scan;
}

} // namespace savevault::services
