pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import EchoMusic.Shell
import EchoMusic.Search

RoutePage {
    id: routePage
    required property SearchModel searchService
    readonly property string resultRoute: searchService.kind === SearchModel.Playlist ? "playlist-detail" : searchService.kind === SearchModel.Artist ? "artist-detail" : searchService.kind === SearchModel.Album ? "album-detail" : searchService.kind === SearchModel.Mv ? "mv-detail" : ""
    SearchPage {
        anchors.fill: parent
        service: routePage.searchService
        pageActive: routePage.pageActive
        resultNavigationAvailable: routePage.resultRoute.length > 0 && routePage.router.available(routePage.resultRoute)
        onResultOpenRequested: id => routePage.router.navigate(routePage.resultRoute, {}, id)
        viewportWidth: routePage.Window.window ? routePage.Window.window.width / routePage.settings.uiScale : width
        uiScale: routePage.settings.uiScale
        onKindPicked: kind => {
            const tabs = ["song", "album", "author", "special", "lyric", "mv"];
            const query = Object.assign({}, routePage.context.query);
            query.tab = tabs[kind];
            routePage.router.replaceQuery(query);
        }
    }
}
