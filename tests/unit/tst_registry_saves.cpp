#include "ServiceFixture.h"

#include "savevault/core/Uuid.h"
#include "savevault/platform/RegistryStore.h"
#include "savevault/services/BackupService.h"
#include "savevault/services/RestoreService.h"

#include <QJsonArray>

using namespace savevault;
using namespace savevault::services;
using namespace savevault::testing;

namespace {

const QString kKey = QStringLiteral("HKCU/Software/Fictional Studio/Fictional Game");

platform::RegistryKeyData keyWith(const QString &key, const QString &level)
{
    return {key,
            {{QStringLiteral("Level"), QStringLiteral("REG_SZ"), level},
             {QStringLiteral("Coins"), QStringLiteral("REG_DWORD"), qint64(4294967295LL)},
             {QStringLiteral("Seed"), QStringLiteral("REG_QWORD"), QStringLiteral("18446744073709551615")},
             {QStringLiteral("Blob"), QStringLiteral("REG_BINARY"), QStringLiteral("AAEC/w==")},
             {QStringLiteral("Names"), QStringLiteral("REG_MULTI_SZ"), QJsonArray{QStringLiteral("a"), QStringLiteral("b")}},
             {QString(), QStringLiteral("REG_SZ"), QStringLiteral("default value")}}};
}

QString valueOf(const QList<platform::RegistryKeyData> &tree, const QString &name)
{
    for (const platform::RegistryValueData &v : tree.first().values) {
        if (v.name == name)
            return v.data.isString() ? v.data.toString() : QString::number(v.data.toInteger());
    }
    return {};
}

} // namespace

class TestRegistrySaves : public QObject {
    Q_OBJECT

    // The fixture's game with a registry rule, on a Windows host using `store`.
    static GameTarget target(ServiceFixture &f)
    {
        GameTarget t = f.target();
        profiles::RegistryRule rule;
        rule.id = QStringLiteral("settings");
        rule.tags = {profiles::Tag::Save};
        rule.keys = {kKey};
        t.profile.registry = {rule};
        return t;
    }

    static ServiceEnvironment windowsEnv(ServiceFixture &f, std::shared_ptr<platform::RegistryStore> store)
    {
        ServiceEnvironment env = f.environment();
        env.hostOs = platform::Os::Windows;
        env.registry = std::move(store);
        return env;
    }

private slots:
    void validatesKeys()
    {
        QVERIFY(platform::isUserSoftwareKey(kKey));
        QVERIFY(platform::isUserSoftwareKey(QStringLiteral("hkcu/software/x")));
        QVERIFY(!platform::isUserSoftwareKey(QStringLiteral("HKLM/Software/x")));
        QVERIFY(!platform::isUserSoftwareKey(QStringLiteral("HKCU/System/x")));
        QVERIFY(!platform::isUserSoftwareKey(QStringLiteral("HKCU/Software")));
        QVERIFY(!platform::isUserSoftwareKey(QStringLiteral("HKCU/Software/../x")));
        platform::MemoryRegistryStore store;
        QCOMPARE(store.writeValues({QStringLiteral("HKLM/Software/x"), {}}).error().code, ErrorCode::PermissionDenied);
    }

    void backsUpAndRestoresRegistryValues()
    {
        ServiceFixture f;
        auto store = std::make_shared<platform::MemoryRegistryStore>();
        QVERIFY(store->writeValues(keyWith(kKey, QStringLiteral("world 3"))).ok());
        QVERIFY(store->writeValues(keyWith(kKey + QStringLiteral("/Profiles/1"), QStringLiteral("p1"))).ok());
        const ServiceEnvironment env = windowsEnv(f, store);

        // Registry-only profile: no files, still backed up.
        const auto snapshot = BackupService(*f.vault, env).backup(target(f));
        QVERIFY2(snapshot.ok(), qPrintable(snapshot.ok() ? QString() : snapshot.error().toString()));
        QCOMPARE(snapshot.value().registry.size(), 2); // the key and its subkey
        QCOMPARE(snapshot.value().registry.first().ruleId, QStringLiteral("settings"));

        // The game changes a value; restoring brings it back and keeps an undo point.
        QVERIFY(store->writeValues({kKey, {{QStringLiteral("Level"), QStringLiteral("REG_SZ"), QStringLiteral("world 9")}}}).ok());
        RestoreService restore(*f.vault, env);
        const auto plan = restore.plan(target(f), snapshot.value().snapshotId);
        QVERIFY(plan.ok());
        QCOMPARE(plan.value().registryKeys.size(), 1); // only the changed key
        QVERIFY(plan.value().hasChanges());
        const auto report = restore.execute(target(f), plan.value(), CloudPolicy::Ask);
        QVERIFY2(report.ok(), qPrintable(report.ok() ? QString() : report.error().toString()));
        QVERIFY(report.value().preRestoreSnapshotId);
        QCOMPARE(valueOf(store->readTree(kKey).value(), QStringLiteral("Level")), QStringLiteral("world 3"));

        // Nothing left to do afterwards.
        QVERIFY(!restore.plan(target(f), snapshot.value().snapshotId).value().hasChanges());
    }

    void skipsRegistryWhereItDoesNotApply()
    {
        ServiceFixture f;
        auto store = std::make_shared<platform::MemoryRegistryStore>();
        QVERIFY(store->writeValues(keyWith(kKey, QStringLiteral("world 3"))).ok());
        writeFile(f.saveDir() + QStringLiteral("/slot.sav"), "x");
        const auto snapshot = BackupService(*f.vault, windowsEnv(f, store)).backup(target(f));
        QVERIFY(snapshot.ok());

        ServiceEnvironment linux = f.environment();
        linux.hostOs = platform::Os::Linux;
        linux.registry = store;
        const auto plan = RestoreService(*f.vault, linux).plan(target(f), snapshot.value().snapshotId);
        QVERIFY(plan.ok());
        QVERIFY(plan.value().registryKeys.isEmpty());
        QCOMPARE(plan.value().skippedRegistryKeys, 1);
        // A Linux backup of the same profile captures the files only.
        QVERIFY(BackupService(*f.vault, linux).backup(target(f)).value().registry.isEmpty());
    }

    void windowsRegistryRoundTrip()
    {
#if !defined(Q_OS_WIN)
        QSKIP("the real registry exists only on Windows");
#else
        auto store = platform::hostRegistryStore();
        QVERIFY(store->available());
        const QString root = QStringLiteral("HKCU/Software/SaveVaultTests/") + uuidToString(createUuidV7());
        QVERIFY(store->writeValues(keyWith(root, QStringLiteral("world 3"))).ok());
        QVERIFY(store->writeValues(keyWith(root + QStringLiteral("/Sub"), QStringLiteral("sub"))).ok());
        const auto tree = store->readTree(root);
        QVERIFY(tree.ok());
        (void)store->removeTree(QStringLiteral("HKCU/Software/SaveVaultTests"));
        QCOMPARE(tree.value().size(), 2);
        QCOMPARE(valueOf(tree.value(), QStringLiteral("Level")), QStringLiteral("world 3"));
        QCOMPARE(valueOf(tree.value(), QStringLiteral("Coins")), QStringLiteral("4294967295"));
        QCOMPARE(valueOf(tree.value(), QStringLiteral("Seed")), QStringLiteral("18446744073709551615"));
        QCOMPARE(valueOf(tree.value(), QStringLiteral("Blob")), QStringLiteral("AAEC/w=="));
        QCOMPARE(valueOf(tree.value(), QString()), QStringLiteral("default value"));
        for (const platform::RegistryValueData &v : tree.value().first().values) {
            if (v.name == u"Names")
                QCOMPARE(v.data.toArray(), (QJsonArray{QStringLiteral("a"), QStringLiteral("b")}));
        }
        QVERIFY(store->readTree(root).value().isEmpty()); // removed
#endif
    }
};

QTEST_GUILESS_MAIN(TestRegistrySaves)
#include "tst_registry_saves.moc"
