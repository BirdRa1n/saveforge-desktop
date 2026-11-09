#include "saveforge/registry/Fetcher.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace saveforge::registry {

namespace {

constexpr int kTransferTimeoutMs = 30'000;

class DirectoryFetcher final : public Fetcher {
public:
    explicit DirectoryFetcher(QString dir)
        : m_dir(std::move(dir))
    {
    }

    Result<QByteArray> fetch(const QString &relativePath, qint64 maxBytes) override
    {
        QFile file(m_dir + u'/' + relativePath);
        if (!file.exists())
            return Error{ErrorCode::NotFound, QStringLiteral("file not found"), file.fileName()};
        if (!file.open(QIODevice::ReadOnly))
            return Error{ErrorCode::Io, file.errorString(), file.fileName()};
        if (file.size() > maxBytes)
            return Error{ErrorCode::Corrupted, QStringLiteral("file is too large"), file.fileName()};
        return file.readAll();
    }

    QString location() const override { return m_dir; }

private:
    QString m_dir;
};

class HttpFetcher final : public Fetcher {
public:
    HttpFetcher(QUrl base, QString userAgent)
        : m_base(std::move(base))
        , m_userAgent(std::move(userAgent))
    {
    }

    Result<QByteArray> fetch(const QString &relativePath, qint64 maxBytes) override
    {
        const QUrl url = m_base.resolved(QUrl(relativePath));
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", m_userAgent.toUtf8());
        request.setTransferTimeout(kTransferTimeoutMs);
        // Follow redirects, but never from HTTPS down to plain HTTP.
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

        // A manager per request keeps the fetcher usable from any worker thread.
        QNetworkAccessManager manager;
        std::unique_ptr<QNetworkReply> reply(manager.get(request));
        bool tooLarge = false;
        QEventLoop loop;
        QObject::connect(reply.get(), &QNetworkReply::downloadProgress, reply.get(),
                         [&tooLarge, &reply, maxBytes](qint64 received, qint64 total) {
                             if (received > maxBytes || total > maxBytes) {
                                 tooLarge = true;
                                 reply->abort();
                             }
                         });
        QObject::connect(reply.get(), &QNetworkReply::finished, &loop, &QEventLoop::quit);
        if (!reply->isFinished())
            loop.exec();

        const QString context = url.toString(QUrl::RemoveUserInfo);
        if (tooLarge)
            return Error{ErrorCode::Corrupted, QStringLiteral("file is too large"), context};
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 404 || status == 410)
            return Error{ErrorCode::NotFound, QStringLiteral("file not found"), context};
        if (reply->error() != QNetworkReply::NoError)
            return Error{ErrorCode::Network, reply->errorString(), context};
        if (status != 200)
            return Error{ErrorCode::Network, QStringLiteral("unexpected HTTP status %1").arg(status), context};
        QByteArray bytes = reply->readAll();
        if (bytes.size() > maxBytes)
            return Error{ErrorCode::Corrupted, QStringLiteral("file is too large"), context};
        return bytes;
    }

    QString location() const override { return m_base.toString(QUrl::RemoveUserInfo); }

private:
    QUrl m_base;
    QString m_userAgent;
};

bool isLoopback(const QString &host)
{
    if (host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0)
        return true;
    const QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}

} // namespace

Result<std::unique_ptr<Fetcher>> makeFetcher(const QString &mirror, const QString &userAgent)
{
    const QString trimmed = mirror.trimmed();
    if (trimmed.isEmpty())
        return Error{ErrorCode::InvalidArgument, QStringLiteral("empty mirror"), {}};

    const QUrl url(trimmed);
    const QString scheme = url.scheme().toLower();
    if (scheme == u"https" || scheme == u"http") {
        if (!url.isValid() || url.host().isEmpty())
            return Error{ErrorCode::InvalidArgument, QStringLiteral("invalid mirror URL"), trimmed};
        // Content is signed either way, but plain HTTP would expose which
        // profiles a user downloads to anyone on the network.
        if (scheme == u"http" && !isLoopback(url.host()))
            return Error{ErrorCode::InvalidArgument, QStringLiteral("mirrors must use HTTPS"), trimmed};
        QUrl base = url;
        if (!base.path().endsWith(u'/'))
            base.setPath(base.path() + u'/');
        return std::unique_ptr<Fetcher>(std::make_unique<HttpFetcher>(base, userAgent));
    }

    // Windows drive letters ("D:/x") parse as a one-letter scheme.
    const bool drivePath = scheme.size() == 1;
    if (scheme == u"file" || scheme.isEmpty() || drivePath) {
        const QString dir = scheme == u"file" ? url.toLocalFile() : trimmed;
        if (!QDir(dir).exists())
            return Error{ErrorCode::NotFound, QStringLiteral("mirror directory not found"), dir};
        return std::unique_ptr<Fetcher>(std::make_unique<DirectoryFetcher>(QDir::cleanPath(QDir::fromNativeSeparators(dir))));
    }
    return Error{ErrorCode::InvalidArgument, QStringLiteral("unsupported mirror scheme"), trimmed};
}

} // namespace saveforge::registry
