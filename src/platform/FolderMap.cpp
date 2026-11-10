#include "savevault/platform/FolderMap.h"

#include <QDir>

#include <algorithm>

namespace savevault::platform {

void FolderMap::set(const QString &placeholder, const QString &absoluteDir)
{
    if (absoluteDir.isEmpty()) {
        m_dirs.remove(placeholder);
        return;
    }
    m_dirs.insert(placeholder, QDir::cleanPath(QDir::fromNativeSeparators(absoluteDir)));
}

void FolderMap::remove(const QString &placeholder)
{
    m_dirs.remove(placeholder);
}

bool FolderMap::contains(const QString &placeholder) const
{
    return m_dirs.contains(placeholder);
}

QString FolderMap::value(const QString &placeholder) const
{
    return m_dirs.value(placeholder);
}

QStringList FolderMap::placeholders() const
{
    QStringList names = m_dirs.keys();
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace savevault::platform
