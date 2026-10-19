#pragma once
#include <QObject>
#include <QTcpServer>
#include <QUrl>
#include <functional>

namespace EchoMusic
{
/// Per-request stream adapter for public libcurl proxy negotiation. Can adopt a
/// route-selected connection or open a TLS proxy stream. Origin TLS remains
/// opaque. No shared authentication or account state is stored here.
class ProxySocketBridge final : public QObject
{
public:
    ProxySocketBridge(QUrl endpoint, std::function<void()> failed, QObject *parent,
                      QTcpSocket *initial = nullptr);
    quint16 port() const;
    void close();

private:
    QTcpServer m_server;
    QTcpSocket *m_initial = nullptr;
    int m_pipeCount = 0;
};
} // namespace EchoMusic
