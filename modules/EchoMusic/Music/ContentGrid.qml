pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Controls

GridView {
    id: grid
    property bool pageActive: true
    property EchoPageScrollContainer pageContainer: null
    property bool artist: false
    property bool video: false
    property bool navigationAvailable: false
    // The view swaps in a bounded placeholder count while loading.
    // The catalog model retains ownership of the real rows.
    property var sourceModel: null
    property bool loading: false
    readonly property int columns: Math.max(1, Math.floor(width / ((video ? 220 : 180) + 20)))
    readonly property real cardWidth: Math.max(1, width / columns - 20)
    readonly property real cardHeight: (cardWidth - (artist ? 26 : video ? 18 : 22)) / (video ? 1.78 : 1) + (artist ? 70 : video ? 62 : 66)
    signal openRequested(string id)
    /// Optional load-more sentinel for paginated grids. Defaults off.
    property bool hasMore: false
    property bool loadingMore: false
    signal loadMoreRequested
    function checkLoadMore(): void {
        if (!grid.hasMore || grid.loadingMore || grid.loading || !grid.visible)
            return;
        if (grid.atYEnd && grid.contentHeight > grid.height)
            grid.loadMoreRequested();
    }
    onMovementEnded: checkLoadMore()
    onAtYEndChanged: checkLoadMore()
    cellWidth: cardWidth + 20
    cellHeight: cardHeight + 20
    clip: true
    reuseItems: true
    cacheBuffer: cellHeight * 3
    boundsBehavior: Flickable.StopAtBounds
    model: grid.loading ? grid.columns * 2 : grid.sourceModel
    ScrollBar.vertical: EchoPageScrollBar {
        objectName: "gridScrollbar"
        view: grid
        container: grid.pageContainer
        viewActive: grid.visible && grid.pageActive
    }
    delegate: grid.loading ? skeletonDelegate : cardDelegate
    Component {
        id: cardDelegate
        ContentCard {
            id: card
            width: grid.cardWidth
            height: grid.cardHeight
            artist: grid.artist
            video: grid.video
            navigationAvailable: grid.navigationAvailable
            property bool pooled: false
            resourceActive: grid.pageActive && !pooled
            GridView.onPooled: pooled = true
            GridView.onReused: pooled = false
            onOpenRequested: id => grid.openRequested(id)
        }
    }
    Component {
        id: skeletonDelegate
        ContentSkeleton {
            property bool pooled: false
            width: grid.cardWidth
            height: grid.cardHeight
            video: grid.video
            resourceActive: grid.pageActive && !pooled
            GridView.onPooled: pooled = true
            GridView.onReused: pooled = false
        }
    }
}
