pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import EchoMusic.Theme

Rectangle {
    id: skeleton
    objectName: "echoSkeleton"
    enum Variant {
        Block,
        Text,
        Circle
    }
    property int variant: EchoSkeleton.Block
    property bool animate: true
    property bool shimmer: true
    readonly property bool animationActive: animate && visible && Theme.store.motionEnabled && Window.window !== null && Window.window.visible && Window.window.visibility !== Window.Minimized
    color: Theme.mutedSurface
    radius: variant === EchoSkeleton.Block ? Theme.cardRadius : Math.min(width, height) / 2
    // A 200%-wide gradient travels from -100% to 0 in 1.35s.
    // Analytic rounded clipping avoids per-placeholder texture capture.
    // NumberAnimation keeps uniforms on the item lifecycle: a render-thread
    // UniformAnimator job can outlive these short-lived materials during retirement.
    Loader {
        anchors.fill: parent
        active: skeleton.shimmer && skeleton.animationActive
        sourceComponent: ShaderEffect {
            id: sweep
            // Use a 5% content tone rather than the opaque card palette.
            property color shimmerColor: Theme.mix(Theme.mutedSurface, Theme.hover, 0.72)
            property vector2d viewSize: Qt.vector2d(width, height)
            property real cornerRadius: skeleton.radius
            property real phase: 0
            fragmentShader: "qrc:/qt/qml/EchoMusic/Controls/shaders/skeleton-shimmer.frag.qsb"
            NumberAnimation {
                target: sweep
                property: "phase"
                from: 0
                to: 1
                duration: 1350
                easing.type: Easing.BezierSpline
                easing.bezierCurve: [0.42, 0, 0.58, 1, 1, 1]
                loops: Animation.Infinite
                running: true
            }
        }
    }
}
