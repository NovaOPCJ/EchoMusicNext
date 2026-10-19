#pragma once
#include "NetworkConfiguration.h"

namespace EchoMusic
{
/// Pure Chromium-compatible manual rule selection. No DNS queries: CIDR bypass
/// applies only to IP literals. An empty list is an error, never implicit DIRECT.
class ProxyRouting
{
public:
    static bool implicitBypass(const QUrl &url);
    static bool bypasses(const QUrl &url, const QString &rules);
    static QList<NetworkProxyEndpoint> manual(const NetworkConfiguration &config, const QUrl &url);
    static QList<NetworkProxyEndpoint> pacResult(const QString &result);
};
} // namespace EchoMusic
