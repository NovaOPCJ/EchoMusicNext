pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import EchoMusic.Controls

// Clip the shadow's interior analytically; translucent surfaces must not show
// black shadow through their own fill. All surfaces share the same RHI shader.
RectangularShadow {
    id: shadow
    required property Item surface
    cached: false
    EchoEffectOwnership {
        surface: shadow
    }
    // Let the ownership adapter retain Qt's default before replacing it,
    // including shadows destroyed without ever being sized or shown.
    Component.onCompleted: material = surfaceMaterial
    ShaderEffect {
        id: surfaceMaterial
        visible: false
        property color color
        property vector3d iResolution
        property point rectSize
        property real radius
        property real blur
        readonly property vector2d sourceSize: Qt.vector2d(shadow.surface.width, shadow.surface.height)
        readonly property point sourceOffset: shadow.offset
        readonly property real sourceRadius: shadow.radius
        vertexShader: "qrc:/qt/qml/EchoMusic/Controls/shaders/action-shadow.vert.qsb"
        fragmentShader: "qrc:/qt/qml/EchoMusic/Controls/shaders/action-shadow.frag.qsb"
    }
}
