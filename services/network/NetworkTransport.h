#pragma once
#include "NetworkConfiguration.h"
#include <QObject>
#include <QThread>
#include <functional>
class QHostAddress;

namespace EchoMusic
{
class NetworkTransportWorker;
/// One loopback HTTP/CONNECT bridge gives Qt, Chromium and Rust the same route
/// decision, including HTTPS proxies and SOCKS4 unavailable in QNetworkProxy.
/// TLS to the origin remains opaque; this service never decrypts origin TLS.
class NetworkTransport final : public QObject
{
    Q_OBJECT
public:
    explicit NetworkTransport(QObject *parent = nullptr);
    ~NetworkTransport() override;
    bool isReady() const;
    QUrl endpoint() const;
    QUrl registerMedia(const QUrl &source);
    /// Session-scoped LAN relay, separate from the loopback proxy. Only opaque
    /// registered media tokens are served; no arbitrary URL/CONNECT interface.
    void registerCastMedia(const QUrl &source, const QHostAddress &receiver,
                           std::function<void(QUrl, QString)> complete);
    void releaseMedia(const QUrl &resource);
    /// Ordered with connection acceptance on the worker. Existing connections
    /// close on a changed policy; non-idempotent application requests aren't replayed.
    void apply(const NetworkConfiguration &configuration, const QString &password);
    void recoverAfterWake();
    void afterPendingChanges(std::function<void()> complete);
signals:
    void mediaRequestRejected(const QString &token, int status);

private:
    QThread m_thread;
    NetworkTransportWorker *m_worker;
    quint16 m_port = 0;
    quint16 m_mediaPort = 0;
};
} // namespace EchoMusic
