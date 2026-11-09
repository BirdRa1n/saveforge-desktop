#pragma once

// A minimal HTTP/1.1 server that serves the files of one directory, for tests
// of code that downloads over HTTP. It answers GET requests on the loopback
// interface with 200 or 404, closes every connection after one response, and
// records the paths it was asked for. It runs on the test's event loop, which
// also spins while a blocking fetcher waits for its reply.

#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

namespace saveforge::test {

class StaticHttpServer : public QObject {
public:
    explicit StaticHttpServer(QString root)
        : m_root(std::move(root))
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket *socket = m_server.nextPendingConnection())
                serve(socket);
        });
    }

    [[nodiscard]] bool listen() { return m_server.listen(QHostAddress::LocalHost); }
    // Base URL with a trailing slash, e.g. "http://127.0.0.1:50123/".
    [[nodiscard]] QString baseUrl() const
    {
        return QStringLiteral("http://127.0.0.1:%1/").arg(m_server.serverPort());
    }
    [[nodiscard]] const QStringList &requests() const { return m_requests; }
    void clearRequests() { m_requests.clear(); }

private:
    void serve(QTcpSocket *socket)
    {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
            m_buffers[socket] += socket->readAll();
            const QByteArray &buffer = m_buffers[socket];
            if (!buffer.contains("\r\n\r\n"))
                return;
            const QList<QByteArray> requestLine = buffer.left(buffer.indexOf("\r\n")).split(' ');
            m_buffers.remove(socket);
            const QString path = QString::fromUtf8(requestLine.value(1)).mid(1);
            m_requests.append(path);

            QFile file(m_root + u'/' + path);
            QByteArray response;
            if (requestLine.value(0) == "GET" && !path.contains(QLatin1String("..")) && file.open(QIODevice::ReadOnly)) {
                const QByteArray body = file.readAll();
                response = "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size())
                         + "\r\nConnection: close\r\n\r\n" + body;
            } else {
                response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            }
            socket->write(response);
            socket->disconnectFromHost();
        });
    }

    QString m_root;
    QTcpServer m_server;
    QStringList m_requests;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

} // namespace saveforge::test
