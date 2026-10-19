pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme

ShaderEffect {
    property int kind: 0
    property real radius: Theme.dialogRadius
    property color accent: Theme.accent
    property color base: Theme.card
    property color ink: Theme.text
    property vector4d geometry: Qt.vector4d(width, height, radius, kind)
    fragmentShader: "qrc:/qt/qml/EchoMusic/ListenTogether/shaders/listen-surface.frag.qsb"
}
