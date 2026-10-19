#pragma once
#include "ProxyResolver.h"
namespace EchoMusic
{
/// Called on resolver workers only. Preserve raw libproxy order/protocols rather
/// than Qt's restricted QNetworkProxy conversion and appended DIRECT candidate.
ProxyResolution resolveLinuxSystemProxy(const QUrl &url, ProxyResolveCancellation cancelled);
} // namespace EchoMusic
