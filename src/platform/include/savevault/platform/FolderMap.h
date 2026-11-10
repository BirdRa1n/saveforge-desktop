#pragma once

#include <QHash>
#include <QString>

namespace savevault::platform {

// Absolute directories for root placeholders, keyed by placeholder name
// without angle brackets (e.g. "winAppData"). Paths always use '/'.
// A placeholder that is absent cannot be resolved on this machine; profile
// paths that start with it are skipped, which is not an error.
class FolderMap {
public:
    void set(const QString &placeholder, const QString &absoluteDir);
    void remove(const QString &placeholder);

    [[nodiscard]] bool contains(const QString &placeholder) const;
    // Empty string when the placeholder is not available.
    [[nodiscard]] QString value(const QString &placeholder) const;
    [[nodiscard]] QStringList placeholders() const;

private:
    QHash<QString, QString> m_dirs;
};

} // namespace savevault::platform
