pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme

Item {
    id: control
    property string name: "music"
    // Callers supply their actual surface/state foreground, like currentColor.
    property color color: Theme.text
    property bool resourceActive: true
    readonly property alias status: image.status
    readonly property int rasterSize: Math.max(1, Math.ceil(Math.min(width, height) * Screen.devicePixelRatio))
    implicitWidth: 20
    implicitHeight: 20
    Image {
        id: image
        objectName: "iconImage"
        anchors.fill: parent
        source: control.resourceActive && control.visible ? EchoIconSource.urlFor(control.name, control.color) : ""
        sourceSize: Qt.size(control.rasterSize, control.rasterSize)
        fillMode: Image.PreserveAspectFit
        asynchronous: true
    }
}
