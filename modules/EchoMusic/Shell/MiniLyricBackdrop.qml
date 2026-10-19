pragma ComponentBehavior: Bound
import QtQuick

// The cover uses 22% opacity beneath an 84–95% surface scrim.
// Compose the full panel and rounded edge in one quad without extra capture layers.
Item {
    id: backdrop
    objectName: "miniLyricBackdrop"
    required property url coverSource
    required property color baseColor
    property real cornerRadius: 15
    property bool roundTop: false
    Image {
        id: cover
        objectName: "miniLyricBackdropCover"
        source: backdrop.coverSource
        sourceSize: Qt.size(512, 512)
        cache: false
        asynchronous: true
        visible: false
    }
    Loader {
        anchors.fill: parent
        active: cover.status === Image.Ready
        sourceComponent: ShaderEffect {
            objectName: "miniLyricBackdropComposite"
            property var source: cover
            property color baseColor: backdrop.baseColor
            property size extent: Qt.size(width, height)
            property real coverAspect: Math.max(1, cover.implicitWidth) / Math.max(1, cover.implicitHeight)
            property real cornerRadius: backdrop.cornerRadius
            property real roundTop: backdrop.roundTop ? 1 : 0
            fragmentShader: "qrc:/qt/qml/EchoMusic/Shell/shaders/mini-lyric-backdrop.frag.qsb"
        }
    }
}
