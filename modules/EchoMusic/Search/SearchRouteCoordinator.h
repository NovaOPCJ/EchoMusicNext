#pragma once
#include <QObject>
#include <QString>

namespace EchoMusic
{
class SearchModel;
class RouteController;

/// Owns search route registration and activation policy, keeping business
/// query/tab handling out of the generic shell and the application entry point.
class SearchRouteCoordinator final : public QObject
{
public:
    SearchRouteCoordinator(SearchModel &search, RouteController &router, bool networkEnabled = true,
                           QObject *parent = nullptr);

private:
    QString m_instanceId;
};
} // namespace EchoMusic
