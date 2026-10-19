#pragma once
#include <QByteArray>
#include <QVariantMap>
#include <QSet>
namespace EchoMusic
{
/// Marketplace statistics response. Ignore unsolicited identities;
/// missing catalog rows remain unknown.
struct PluginMarketplaceStats
{
    static QVariantMap parse(const QByteArray &bytes, const QSet<QString> &requested,
                             QString &error);
};
} // namespace EchoMusic
