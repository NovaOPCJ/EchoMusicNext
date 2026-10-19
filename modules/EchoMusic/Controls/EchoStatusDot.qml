pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme

// Current-state marker with a solid dot and an unblurred outer halo.
Item {
    id: dot
    property color color: Theme.accent
    property real haloWidth: 3
    property color haloColor: Theme.withAlpha(color, 0.14)
    property string accessibleName: ""
    implicitWidth: 5
    implicitHeight: 5
    Accessible.role: Accessible.Graphic
    Accessible.name: accessibleName
    Accessible.ignored: accessibleName.length === 0
    Rectangle {
        objectName: "statusDotHalo"
        anchors.fill: parent
        anchors.margins: -dot.haloWidth
        radius: height / 2
        color: dot.haloColor
    }
    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: dot.color
    }
}
