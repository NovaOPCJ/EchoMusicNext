pragma ComponentBehavior: Bound
import QtQuick

// A uniform grid region in one continuous page. Only the outer Flickable owns
// input/inertia. The native GridView projects a bounded viewport into this
// region's exact extent; it never expands to the height of the whole catalog.
Item {
    id: section
    required property Flickable scrollView
    required property real contentTop
    required property var model
    required property int itemCount
    required property Component delegate
    property int columns: 1
    property real cellHeight: 72
    property bool resourceActive: true
    readonly property real cellWidth: width / Math.max(1, columns)
    readonly property real localTop: scrollView.contentY - contentTop
    readonly property bool nearViewport: localTop + scrollView.height > -cellHeight * 2 && localTop < height + cellHeight * 2
    readonly property GridView view: viewport.item as GridView
    implicitHeight: Math.ceil(itemCount / Math.max(1, columns)) * cellHeight
    height: implicitHeight
    // Model and delegate are a pair. A ready -> loading transition changes
    // both bindings; retire the old view before it can instantiate real cards
    // against the placeholder integer model (or the reverse).
    onModelChanged: if (projectionState && projectionState.completed) {
        projectionState.updating = true;
        rebuild.restart();
    }
    onDelegateChanged: if (projectionState && projectionState.completed) {
        projectionState.updating = true;
        rebuild.restart();
    }
    Component.onCompleted: projectionState.completed = true
    QtObject {
        id: projectionState
        property bool completed: false
        property bool updating: false
    }
    Timer {
        id: rebuild
        interval: 0
        onTriggered: projectionState.updating = false
    }
    Loader {
        id: viewport
        active: section.resourceActive && section.nearViewport && section.height > 0 && width > 0 && !projectionState.updating
        width: section.width
        height: Math.min(section.height, section.scrollView.height)
        y: Math.max(0, Math.min(section.localTop, section.height - height))
        function syncPosition(): void {
            const grid = item as GridView;
            if (grid && Math.abs(grid.contentY - y) > .001)
                grid.contentY = y;
        }
        onYChanged: syncPosition()
        // Native component completion/model resets can restore the view's own
        // origin. This non-interactive view only projects the outer position.
        onLoaded: syncPosition()
        sourceComponent: GridView {
            id: grid
            model: section.model
            delegate: section.delegate
            cellWidth: section.cellWidth
            cellHeight: section.cellHeight
            onContentYChanged: if (Math.abs(contentY - viewport.y) > .001)
                placement.restart()
            onCountChanged: placement.restart()
            currentIndex: -1
            interactive: false
            keyNavigationEnabled: false
            reuseItems: true
            cacheBuffer: cellHeight * 2
            // Cached cells keep their original section coordinates. The outer
            // page clips them; a second clip here would cut off card hover lift
            // at the beginning of a section.
            clip: false
            boundsBehavior: Flickable.StopAtBounds
            Timer {
                id: placement
                interval: 0
                onTriggered: viewport.syncPosition()
            }
        }
    }
}
