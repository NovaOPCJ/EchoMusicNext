#pragma once
#include <QObject>
namespace EchoMusic
{
class DiscoveryService;
class RouteController;
/// The router owns mounted page contexts; this coordinator owns no duplicate data.
class DiscoveryRouteCoordinator final : public QObject
{
    Q_OBJECT
public:
    DiscoveryRouteCoordinator(DiscoveryService &, RouteController &, bool networkEnabled = true,
                              QObject *parent = nullptr);
};
} // namespace EchoMusic
