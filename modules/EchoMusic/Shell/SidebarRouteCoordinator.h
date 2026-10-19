#pragma once
#include <QObject>
#include <QString>

namespace EchoMusic
{
class LibraryController;
class RouteController;
class UserPlaylistModel;
/// Reveal the current playlist's catalog section after any navigation, including
/// history traversal and a catalog that arrives after the route. Owns no tab state.
class SidebarRouteCoordinator final : public QObject
{
    Q_OBJECT
public:
    SidebarRouteCoordinator(LibraryController &library, UserPlaylistModel &source,
                            RouteController &router, QObject *parent = nullptr);

private:
    void routeChanged();
    void synchronize();
    LibraryController &m_library;
    UserPlaylistModel &m_source;
    RouteController &m_router;
    QString m_routeName, m_resourceId;
};
} // namespace EchoMusic
