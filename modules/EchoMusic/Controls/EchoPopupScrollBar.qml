pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import QtQuick.Templates as T

// Declare as ScrollBar.vertical on the original Flickable/ScrollView so Qt
// owns orientation, size, position and drag. The view property only maps visual
// geometry; declaring this as an ordinary sibling does not attach it to the view.
// Only the visual track leaves its padded/clipped content viewport. Like
// EchoPageScrollBar, its right edge is the surface edge; EchoScrollBar owns the
// shared thumb inset. No popup-specific extra right margin is added.
EchoScrollBar {
    id: bar
    objectName: "popupScrollBar"
    required property T.Popup surface
    required property Item view
    wheelView: (view as Flickable) || ((view as T.ScrollView) ? (view as T.ScrollView).contentItem as Flickable : null)
    angleWheelEnabled: surface.visible
    readonly property EchoFloatingSurface frame: surface.visible ? surface.background as EchoFloatingSurface : null
    // The thumb lives beside the content layer. Keep its ends clear of an
    // outer corner when a flush viewport reaches the panel edge.
    topPadding: Math.max(6, frame ? frame.radius - Math.max(0, geometry.rect.y - frame.y) : 0)
    bottomPadding: Math.max(6, frame ? frame.radius - Math.max(0, frame.y + frame.height - geometry.rect.y - geometry.rect.height) : 0)
    parent: surface.contentItem ? surface.contentItem.parent : null
    hoverTarget: surface.contentItem
    x: parent ? parent.width - width : 0
    y: geometry.rect.y
    height: geometry.rect.height
    z: 160
    EchoPopupAnchor {
        id: geometry
        item: bar.view
        coordinateItem: bar.parent
    }
}
