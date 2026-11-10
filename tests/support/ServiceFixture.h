#pragma once

// Shared fixture for service tests: a temporary "machine" with Windows-style
// user folders, a vault, a profile for a fictional game and a fake process list.

#include "savevault/services/GameTarget.h"
#include "savevault/vault/Vault.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace savevault::testing {

inline void writeFile(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(content), content.size());
}

inline QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

struct ServiceFixture {
    QTemporaryDir root;
    std::optional<vault::Vault> vault;
    QList<platform::ProcessInfo> processes;

    ServiceFixture()
    {
        auto opened = vault::Vault::open(root.path() + QStringLiteral("/vault"));
        if (opened.ok())
            vault.emplace(std::move(opened).value());
        QDir().mkpath(appData());
        QDir().mkpath(installDir());
    }

    QString appData() const { return root.path() + QStringLiteral("/AppData/Roaming"); }
    QString installDir() const { return root.path() + QStringLiteral("/Games/Fictional Game"); }
    QString saveDir() const { return appData() + QStringLiteral("/Fictional Studio/Fictional Game"); }

    services::ServiceEnvironment environment()
    {
        services::ServiceEnvironment env;
        env.machineId = QUuid::fromString(QStringLiteral("7f3e2d1c-0b9a-4876-9543-210fedcba987"));
        env.appVersion = QStringLiteral("0.1.0");
        env.processes = [this] { return processes; };
        return env;
    }

    services::GameTarget target(bool requireGameClosed = false) const
    {
        services::GameTarget t;
        t.profile.id = QStringLiteral("fictional-game");
        t.profile.name = QStringLiteral("Fictional Game");
        t.profile.revision = 2;
        t.profile.identifiers.steam = {999999};
        profiles::SaveRule rule;
        rule.id = QStringLiteral("progress");
        rule.tags = {profiles::Tag::Save};
        rule.paths = {QStringLiteral("<winAppData>/Fictional Studio/Fictional Game/*.sav")};
        t.profile.saves = {rule};
        t.profile.mechanism.type = QStringLiteral("standard");
        t.profile.backup.requireGameClosed = requireGameClosed;
        t.profile.backup.processNames = {QStringLiteral("FictionalGame.exe")};

        t.context.gameOs = platform::Os::Windows;
        t.context.store = QStringLiteral("steam");
        t.context.caseInsensitive = true;
        t.context.folders.set(QStringLiteral("winAppData"), appData());
        t.context.folders.set(QStringLiteral("base"), installDir());
        return t;
    }
};

} // namespace savevault::testing
