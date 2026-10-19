pragma ComponentBehavior: Bound
import QtQuick

// Declare as ScrollBar.vertical on the real Flickable. Reparenting the visual
// track must not replace Qt's attached size/position/drag mapping.
EchoScrollBar {
    id: bar
    required property Flickable view
    property EchoPageScrollContainer container: null
    property bool viewActive: true
    parent: container ? container.scrollbarLayer : view
    hoverTarget: container ? container : view
    x: parent.width - width
    y: 0
    height: parent.height
    z: 160
    resourceActive: viewActive && (!container || (container.pageActive && !container.hideScrollbar && container.scrollView === view))
}
