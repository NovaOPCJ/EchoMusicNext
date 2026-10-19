#pragma once
#include "NetworkConfiguration.h"
#include <QObject>
#include <QDeadlineTimer>
#include <QHostAddress>
#include <atomic>
#include <memory>

namespace EchoMusic
{
using ProxyResolveCancellation = std::shared_ptr<std::atomic_bool>;
/// DNS helpers exposed only to a worker-local PAC engine, never the QML engine.
class ProxyPacDns final : public QObject
{
    Q_OBJECT
public:
    explicit ProxyPacDns(ProxyResolveCancellation cancelled = {},
                         QDeadlineTimer deadline = QDeadlineTimer(2000))
        : m_cancelled(std::move(cancelled)), m_deadline(deadline)
    {
    }
    Q_INVOKABLE QString dnsResolve(const QString &host);
    Q_INVOKABLE QString dnsResolveEx(const QString &host);
    Q_INVOKABLE QString myIpAddress();
    Q_INVOKABLE QString myIpAddressEx();
    Q_INVOKABLE bool isInNetEx(const QString &address, const QString &prefix);

private:
    QList<QHostAddress> lookup(const QString &host);
    ProxyResolveCancellation m_cancelled;
    QDeadlineTimer m_deadline;
};
struct ProxyResolution
{
    QList<NetworkProxyEndpoint> endpoints;
    QString error;
};
/// Blocking resolver used exclusively on bounded transport workers. Qt system
/// resolution can run WPAD/PAC and DNS; it must never run in a GUI proxy factory.
class ProxyResolver
{
public:
    static ProxyResolution resolve(const NetworkConfiguration &config, QUrl target,
                                   ProxyResolveCancellation cancelled = {});
    static ProxyResolution evaluatePac(const QString &script, QUrl target,
                                       ProxyResolveCancellation cancelled = {});
    static void invalidate();
};
} // namespace EchoMusic
