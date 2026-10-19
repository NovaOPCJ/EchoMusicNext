#pragma once
#include "NetworkConfiguration.h"
#include <QElapsedTimer>
#include <QHash>
namespace EchoMusic
{
/// Worker-only transient state. Failed proxies move behind healthy candidates
/// for five minutes; they are never removed and DIRECT is never invented.
class ProxyFailureCache
{
public:
    ProxyFailureCache()
    {
        m_clock.start();
    }
    void clear()
    {
        m_failed.clear();
    }
    void failed(const NetworkProxyEndpoint &endpoint)
    {
        if (endpoint.kind == NetworkProxyEndpoint::Kind::Direct)
            return;
        if (m_failed.size() >= 512 && !m_failed.contains(key(endpoint))) {
            auto oldest = m_failed.begin();
            for (auto it = m_failed.begin(); it != m_failed.end(); ++it)
                if (it.value() < oldest.value())
                    oldest = it;
            m_failed.erase(oldest);
        }
        m_failed.insert(key(endpoint), m_clock.elapsed() + 300000);
    }
    QList<NetworkProxyEndpoint> prioritize(const QList<NetworkProxyEndpoint> &endpoints)
    {
        QList<NetworkProxyEndpoint> healthy, bad;
        const auto now = m_clock.elapsed();
        for (auto it = m_failed.begin(); it != m_failed.end();)
            if (it.value() <= now)
                it = m_failed.erase(it);
            else
                ++it;
        for (const auto &endpoint : endpoints)
            (m_failed.contains(key(endpoint)) ? bad : healthy).append(endpoint);
        healthy += bad;
        return healthy;
    }

private:
    static QString key(const NetworkProxyEndpoint &endpoint)
    {
        const auto defaultPort = endpoint.kind == NetworkProxyEndpoint::Kind::Http    ? 80
                                 : endpoint.kind == NetworkProxyEndpoint::Kind::Https ? 443
                                                                                      : 1080;
        return QString::number(int(endpoint.kind)) + '/' + endpoint.address.host().toLower() + ':' +
               QString::number(endpoint.address.port(defaultPort));
    }
    QElapsedTimer m_clock;
    QHash<QString, qint64> m_failed;
};
} // namespace EchoMusic
