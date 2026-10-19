#include "ProxySocketBridge.h"
#include "NetworkSocketBuffer.h"
#include <QNetworkProxy>
#include <QSslSocket>
#include <QLoggingCategory>
#include <utility>

namespace EchoMusic
{
namespace
{
constexpr qint64 bufferLimit = 128 * 1024;
Q_LOGGING_CATEGORY(proxySocketLog, "echo.network.proxy-socket")
class ProxySocketPipe final : public QObject
{
public:
    ProxySocketPipe(QTcpSocket *local, const QUrl &endpoint, std::function<void()> failed,
                    QObject *parent, QTcpSocket *initial)
        : QObject(parent), m_local(local),
          m_remote(initial                        ? initial
                   : endpoint.scheme() == "https" ? new QSslSocket(this)
                                                  : new QTcpSocket(this))
    {
        local->setParent(this);
        m_remote->setParent(this);
        local->setReadBufferSize(bufferLimit);
        m_remote->setReadBufferSize(bufferLimit);
        m_remote->setProxy(QNetworkProxy::NoProxy);
        connect(local, &QTcpSocket::readyRead, this, [this] { send(); });
        connect(m_remote, &QTcpSocket::bytesWritten, this, [this] { send(); });
        connect(m_remote, &QTcpSocket::readyRead, this, [this] { receive(); });
        connect(local, &QTcpSocket::bytesWritten, this, [this] { receive(); });
        if (auto *ssl = qobject_cast<QSslSocket *>(m_remote); endpoint.scheme() == "https" && ssl) {
            connect(ssl, &QSslSocket::encrypted, this, [this] {
                m_ready = true;
                send();
            });
            connect(ssl, &QSslSocket::encryptedBytesWritten, this, [this] { send(); });
            connect(ssl, &QSslSocket::sslErrors, this, [](const QList<QSslError> &errors) {
                for (const auto &error : errors)
                    qCWarning(proxySocketLog)
                        << "Proxy TLS validation failed" << error.errorString();
            });
            m_ready = ssl->isEncrypted();
        } else {
            m_ready = m_remote->state() == QAbstractSocket::ConnectedState;
            connect(m_remote, &QTcpSocket::connected, this, [this] {
                m_ready = true;
                send();
            });
        }
        connect(local, &QTcpSocket::disconnected, this, [this] {
            m_remote->abort();
            deleteLater();
        });
        connect(m_remote, &QTcpSocket::disconnected, this, [this] {
            m_closed = true;
            receive();
        });
        connect(m_remote, &QTcpSocket::errorOccurred, this,
                [this, failed = std::move(failed)](QAbstractSocket::SocketError error) {
                    if (error == QAbstractSocket::RemoteHostClosedError)
                        return;
                    // Keep certificate validation enabled. Do not let an outer
                    // TLS failure turn into an authentication reconnect loop.
                    m_local->abort();
                    failed();
                    deleteLater();
                });
        if (!initial) {
            if (endpoint.scheme() == "https")
                qobject_cast<QSslSocket *>(m_remote)->connectToHostEncrypted(
                    endpoint.host(), quint16(endpoint.port(443)));
            else
                m_remote->connectToHost(endpoint.host(), quint16(endpoint.port(80)));
        }
        send();
        receive();
    }

private:
    void send()
    {
        const auto pending = pendingSocketWrite(m_remote);
        if (m_ready && !m_closed && m_remote->state() == QAbstractSocket::ConnectedState &&
            m_local->bytesAvailable() && pending < bufferLimit)
            m_remote->write(m_local->read(bufferLimit - pending));
    }
    void receive()
    {
        if (m_local->state() != QAbstractSocket::ConnectedState)
            return;
        if (m_remote->bytesAvailable() && m_local->bytesToWrite() < bufferLimit)
            m_local->write(m_remote->read(bufferLimit - m_local->bytesToWrite()));
        if (m_closed && !m_remote->bytesAvailable())
            m_local->disconnectFromHost();
    }
    QTcpSocket *m_local;
    QTcpSocket *m_remote;
    bool m_closed = false, m_ready = false;
};
} // namespace
ProxySocketBridge::ProxySocketBridge(QUrl endpoint, std::function<void()> failed, QObject *parent,
                                     QTcpSocket *initial)
    : QObject(parent), m_server(this), m_initial(initial)
{
    if (initial)
        initial->setParent(this);
    m_server.setProxy(QNetworkProxy::NoProxy);
    m_server.setMaxPendingConnections(1);
    connect(&m_server, &QTcpServer::newConnection, this,
            [this, endpoint = std::move(endpoint), failed = std::move(failed)] {
                while (m_server.hasPendingConnections()) {
                    auto *local = m_server.nextPendingConnection();
                    // A normal auth retry may reconnect while the prior TLS
                    // pipe is draining. Bound live pipes independently of the
                    // route/connection timeouts.
                    if (m_pipeCount >= 4) {
                        local->abort();
                        local->deleteLater();
                        failed();
                    } else {
                        auto *initial = std::exchange(m_initial, nullptr);
                        if (initial && initial->state() != QAbstractSocket::ConnectedState) {
                            initial->deleteLater();
                            initial = nullptr;
                        }
                        auto *pipe = new ProxySocketPipe(local, endpoint, failed, this, initial);
                        ++m_pipeCount;
                        connect(pipe, &QObject::destroyed, this, [this] { --m_pipeCount; });
                    }
                }
            });
    m_server.listen(QHostAddress::LocalHost, 0);
}
quint16 ProxySocketBridge::port() const
{
    return m_server.serverPort();
}
void ProxySocketBridge::close()
{
    m_server.close();
    for (auto *socket : findChildren<QAbstractSocket *>())
        socket->abort();
}
} // namespace EchoMusic
