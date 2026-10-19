#include "SearchRouteCoordinator.h"
#include "SearchModel.h"
#include "RouteController.h"

namespace EchoMusic
{
SearchRouteCoordinator::SearchRouteCoordinator(SearchModel &search, RouteController &router,
                                               bool networkEnabled, QObject *parent)
    : QObject(parent)
{
    router.registerPage({"search",
                         "/main/search",
                         QUrl("qrc:/qt/qml/EchoMusic/Search/SearchRoutePage.qml"),
                         {"tab"},
                         false,
                         false,
                         false,
                         {{"searchService", QVariant::fromValue(&search)}}});
    connect(&router, &RouteController::pageActivated, this,
            [this, &search, &router, networkEnabled](RouteContext *page) {
                const bool active = page->name() == "search";
                // Search discovery belongs to the titlebar popup. A route with
                // no query has no page content and must return to the homepage.
                if (active && page->query().value("q").toString().trimmed().isEmpty() &&
                    router.available(router.homeRoute())) {
                    search.setPageActive(false);
                    router.navigateHome(true);
                    return;
                }
                search.setPageActive(active);
                if (!active || !networkEnabled)
                    return;
                const auto keyword = page->query().value("q").toString();
                const QStringList tabs{"song", "album", "author", "special", "lyric", "mv"};
                const auto kind = SearchModel::Kind(
                    qMax(0, tabs.indexOf(page->query().value("tab", "song").toString())));
                if (keyword.isEmpty()) {
                    search.showDiscovery();
                    if (!m_instanceId.isEmpty() && m_instanceId != page->instanceId())
                        search.reloadDiscovery();
                } else if (m_instanceId != page->instanceId() || search.keyword() != keyword) {
                    search.search(keyword, kind);
                } else {
                    search.setKind(kind);
                }
                m_instanceId = page->instanceId();
            });
}
} // namespace EchoMusic
