#include "saveforge/platform/RegistryStore.h"

#include <QJsonArray>
#include <QStringList>

#include <cstring>
#include <optional>

#if defined(Q_OS_WIN)
#include <qt_windows.h>
#endif

namespace saveforge::platform {

namespace {

constexpr int kMaxDepth = 32;
constexpr qsizetype kMaxKeys = 10'000;

Error invalidKey(const QString &key)
{
    return Error{ErrorCode::PermissionDenied, QStringLiteral("only keys under HKCU/Software can be used"), key};
}

class UnavailableRegistryStore final : public RegistryStore {
public:
    bool available() const override { return false; }
    Result<QList<RegistryKeyData>> readTree(const QString &) const override { return QList<RegistryKeyData>{}; }
    Result<void> writeValues(const RegistryKeyData &key) override
    {
        return Error{ErrorCode::Unsupported, QStringLiteral("this system has no Windows registry"), key.key};
    }
    Result<void> removeTree(const QString &) override { return {}; }
};

#if defined(Q_OS_WIN)

// "HKCU/Software/A/B" -> L"Software\\A\\B"
std::wstring subPath(const QString &key)
{
    QString path = key.mid(key.indexOf(u'/') + 1);
    path.replace(u'/', u'\\');
    return path.toStdWString();
}

Error winError(LONG code, const QString &what, const QString &key)
{
    return Error{code == ERROR_ACCESS_DENIED ? ErrorCode::PermissionDenied : ErrorCode::Io,
                 what + QStringLiteral(" (Windows error %1)").arg(code), key};
}

class WindowsRegistryStore final : public RegistryStore {
public:
    bool available() const override { return true; }

    Result<QList<RegistryKeyData>> readTree(const QString &key) const override
    {
        if (!isUserSoftwareKey(key))
            return invalidKey(key);
        QList<RegistryKeyData> out;
        HKEY handle = nullptr;
        const LONG opened = RegOpenKeyExW(HKEY_CURRENT_USER, subPath(key).c_str(), 0, KEY_READ, &handle);
        if (opened == ERROR_FILE_NOT_FOUND)
            return out;
        if (opened != ERROR_SUCCESS)
            return winError(opened, QStringLiteral("cannot open registry key"), key);
        auto read = readKey(handle, key, 0, out);
        RegCloseKey(handle);
        if (!read.ok())
            return read.error();
        return out;
    }

    Result<void> writeValues(const RegistryKeyData &data) override
    {
        if (!isUserSoftwareKey(data.key))
            return invalidKey(data.key);
        HKEY handle = nullptr;
        const LONG created = RegCreateKeyExW(HKEY_CURRENT_USER, subPath(data.key).c_str(), 0, nullptr, 0, KEY_WRITE,
                                             nullptr, &handle, nullptr);
        if (created != ERROR_SUCCESS)
            return winError(created, QStringLiteral("cannot create registry key"), data.key);
        for (const RegistryValueData &value : data.values) {
            auto written = writeValue(handle, value);
            if (!written.ok()) {
                RegCloseKey(handle);
                return Error{written.error().code, written.error().message, data.key + u'/' + value.name};
            }
        }
        RegCloseKey(handle);
        return {};
    }

    Result<void> removeTree(const QString &key) override
    {
        if (!isUserSoftwareKey(key))
            return invalidKey(key);
        const LONG removed = RegDeleteTreeW(HKEY_CURRENT_USER, subPath(key).c_str());
        if (removed == ERROR_SUCCESS || removed == ERROR_FILE_NOT_FOUND) {
            RegDeleteKeyW(HKEY_CURRENT_USER, subPath(key).c_str());
            return {};
        }
        return winError(removed, QStringLiteral("cannot delete registry key"), key);
    }

private:
    static Result<void> readKey(HKEY handle, const QString &key, int depth, QList<RegistryKeyData> &out)
    {
        if (depth > kMaxDepth || out.size() >= kMaxKeys)
            return Error{ErrorCode::Corrupted, QStringLiteral("registry key tree is too large"), key};
        DWORD subkeys = 0, maxSubkeyLen = 0, values = 0, maxNameLen = 0, maxDataLen = 0;
        if (RegQueryInfoKeyW(handle, nullptr, nullptr, nullptr, &subkeys, &maxSubkeyLen, nullptr, &values, &maxNameLen,
                             &maxDataLen, nullptr, nullptr) != ERROR_SUCCESS)
            return Error{ErrorCode::Io, QStringLiteral("cannot query registry key"), key};

        RegistryKeyData data;
        data.key = key;
        std::wstring name(maxNameLen + 1, L'\0');
        QByteArray buffer(static_cast<qsizetype>(maxDataLen) + 2, '\0');
        for (DWORD i = 0; i < values; ++i) {
            DWORD nameLen = static_cast<DWORD>(name.size());
            DWORD dataLen = static_cast<DWORD>(buffer.size());
            DWORD type = 0;
            if (RegEnumValueW(handle, i, name.data(), &nameLen, nullptr, &type,
                              reinterpret_cast<BYTE *>(buffer.data()), &dataLen) != ERROR_SUCCESS)
                continue;
            const QString valueName = QString::fromWCharArray(name.data(), static_cast<qsizetype>(nameLen));
            const QByteArray bytes = buffer.left(static_cast<qsizetype>(dataLen));
            if (auto value = toValue(valueName, type, bytes))
                data.values.append(*value);
        }
        out.append(data);

        std::wstring subName(maxSubkeyLen + 1, L'\0');
        QStringList children;
        for (DWORD i = 0; i < subkeys; ++i) {
            DWORD len = static_cast<DWORD>(subName.size());
            if (RegEnumKeyExW(handle, i, subName.data(), &len, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
                children.append(QString::fromWCharArray(subName.data(), static_cast<qsizetype>(len)));
        }
        for (const QString &child : std::as_const(children)) {
            HKEY childHandle = nullptr;
            if (RegOpenKeyExW(handle, child.toStdWString().c_str(), 0, KEY_READ, &childHandle) != ERROR_SUCCESS)
                continue;
            auto read = readKey(childHandle, key + u'/' + child, depth + 1, out);
            RegCloseKey(childHandle);
            if (!read.ok())
                return read;
        }
        return {};
    }

    static QString utf16(const QByteArray &bytes)
    {
        QString text = QString::fromWCharArray(reinterpret_cast<const wchar_t *>(bytes.constData()), bytes.size() / 2);
        while (text.endsWith(QChar(u'\0')))
            text.chop(1);
        return text;
    }

    static std::optional<RegistryValueData> toValue(const QString &name, DWORD type, const QByteArray &bytes)
    {
        switch (type) {
        case REG_SZ:
            return RegistryValueData{name, QStringLiteral("REG_SZ"), utf16(bytes)};
        case REG_EXPAND_SZ:
            return RegistryValueData{name, QStringLiteral("REG_EXPAND_SZ"), utf16(bytes)};
        case REG_MULTI_SZ: {
            QStringList parts = utf16(bytes).split(QChar(u'\0'));
            while (!parts.isEmpty() && parts.last().isEmpty())
                parts.removeLast();
            return RegistryValueData{name, QStringLiteral("REG_MULTI_SZ"), QJsonArray::fromStringList(parts)};
        }
        case REG_DWORD: {
            if (bytes.size() < 4)
                return std::nullopt;
            quint32 v = 0;
            std::memcpy(&v, bytes.constData(), 4);
            return RegistryValueData{name, QStringLiteral("REG_DWORD"), static_cast<qint64>(v)};
        }
        case REG_QWORD: {
            if (bytes.size() < 8)
                return std::nullopt;
            quint64 v = 0;
            std::memcpy(&v, bytes.constData(), 8);
            return RegistryValueData{name, QStringLiteral("REG_QWORD"), QString::number(v)};
        }
        case REG_BINARY:
            return RegistryValueData{name, QStringLiteral("REG_BINARY"), QString::fromLatin1(bytes.toBase64())};
        default:
            return std::nullopt;
        }
    }

    static Result<void> writeValue(HKEY handle, const RegistryValueData &value)
    {
        const std::wstring name = value.name.toStdWString();
        QByteArray bytes;
        DWORD type = 0;
        const auto fromText = [](const QString &text) {
            const std::wstring w = text.toStdWString();
            return QByteArray(reinterpret_cast<const char *>(w.c_str()), static_cast<qsizetype>((w.size() + 1) * sizeof(wchar_t)));
        };
        if (value.type == u"REG_SZ" || value.type == u"REG_EXPAND_SZ") {
            type = value.type == u"REG_SZ" ? REG_SZ : REG_EXPAND_SZ;
            bytes = fromText(value.data.toString());
        } else if (value.type == u"REG_MULTI_SZ") {
            type = REG_MULTI_SZ;
            for (const QJsonValue &part : value.data.toArray())
                bytes += fromText(part.toString());
            bytes += QByteArray(sizeof(wchar_t), '\0');
        } else if (value.type == u"REG_DWORD") {
            type = REG_DWORD;
            const quint32 v = static_cast<quint32>(value.data.toInteger());
            bytes = QByteArray(reinterpret_cast<const char *>(&v), 4);
        } else if (value.type == u"REG_QWORD") {
            type = REG_QWORD;
            bool ok = false;
            const quint64 v = value.data.toString().toULongLong(&ok);
            if (!ok)
                return Error{ErrorCode::InvalidArgument, QStringLiteral("invalid REG_QWORD value"), value.name};
            bytes = QByteArray(reinterpret_cast<const char *>(&v), 8);
        } else if (value.type == u"REG_BINARY") {
            type = REG_BINARY;
            bytes = QByteArray::fromBase64(value.data.toString().toLatin1());
        } else {
            return Error{ErrorCode::Unsupported, QStringLiteral("unsupported registry value type"), value.type};
        }
        const LONG set = RegSetValueExW(handle, name.c_str(), 0, type, reinterpret_cast<const BYTE *>(bytes.constData()),
                                        static_cast<DWORD>(bytes.size()));
        if (set != ERROR_SUCCESS)
            return winError(set, QStringLiteral("cannot write registry value"), value.name);
        return {};
    }
};

#endif

} // namespace

bool isUserSoftwareKey(const QString &key)
{
    const QStringList segments = key.split(u'/');
    if (segments.size() < 3 || segments.at(0).compare(QLatin1String("HKCU"), Qt::CaseInsensitive) != 0
        || segments.at(1).compare(QLatin1String("Software"), Qt::CaseInsensitive) != 0)
        return false;
    for (const QString &segment : segments) {
        if (segment.isEmpty() || segment == u"." || segment == u".." || segment.contains(u'\\'))
            return false;
    }
    return true;
}

std::shared_ptr<RegistryStore> hostRegistryStore()
{
#if defined(Q_OS_WIN)
    return std::make_shared<WindowsRegistryStore>();
#else
    return std::make_shared<UnavailableRegistryStore>();
#endif
}

Result<QList<RegistryKeyData>> MemoryRegistryStore::readTree(const QString &key) const
{
    if (!isUserSoftwareKey(key))
        return invalidKey(key);
    QList<RegistryKeyData> out;
    const QString lower = key.toLower();
    for (auto it = m_keys.cbegin(); it != m_keys.cend(); ++it) {
        if (it.key() == lower || it.key().startsWith(lower + u'/'))
            out.append(it.value());
    }
    return out;
}

Result<void> MemoryRegistryStore::writeValues(const RegistryKeyData &data)
{
    if (!isUserSoftwareKey(data.key))
        return invalidKey(data.key);
    RegistryKeyData &stored = m_keys[data.key.toLower()];
    if (stored.key.isEmpty())
        stored.key = data.key;
    for (const RegistryValueData &value : data.values) {
        bool replaced = false;
        for (RegistryValueData &existing : stored.values) {
            if (existing.name.compare(value.name, Qt::CaseInsensitive) == 0) {
                existing = value;
                replaced = true;
            }
        }
        if (!replaced)
            stored.values.append(value);
    }
    return {};
}

Result<void> MemoryRegistryStore::removeTree(const QString &key)
{
    if (!isUserSoftwareKey(key))
        return invalidKey(key);
    const QString lower = key.toLower();
    for (auto it = m_keys.begin(); it != m_keys.end();) {
        if (it.key() == lower || it.key().startsWith(lower + u'/'))
            it = m_keys.erase(it);
        else
            ++it;
    }
    return {};
}

} // namespace saveforge::platform
