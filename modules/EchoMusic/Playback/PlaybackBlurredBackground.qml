pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import EchoMusic.Theme

// Bound the complete offscreen chain in physical pixels. Only its final quad
// fills the window; increasing screen DPR never creates a full-window blur FBO.
Item {
    id: surface
    required property url coverSource
    required property bool rhythm
    required property real phase
    required property color baseColor
    required property real backgroundOpacity
    readonly property bool ready: cover.status === Image.Ready
    readonly property real renderDpr: Math.max(1, Screen.devicePixelRatio)
    readonly property size textureSize: {
        const ratio = Math.min(1, 512 / Math.max(1, width * renderDpr, height * renderDpr));
        return Qt.size(Math.max(1, Math.round(width * renderDpr * ratio)), Math.max(1, Math.round(height * renderDpr * ratio)));
    }
    readonly property real sampleWidth: textureSize.width / renderDpr
    readonly property real sampleHeight: textureSize.height / renderDpr
    readonly property real surfaceScale: sampleWidth / Math.max(1, width)
    onSurfaceScaleChanged: if (renderer.item)
        renderer.item.refresh()
    Image {
        id: cover
        objectName: "playbackBackgroundCover"
        width: surface.sampleWidth
        height: surface.sampleHeight
        source: surface.coverSource
        sourceSize: Qt.size(512, 512)
        // These decoded variants belong to this view, not the global Image
        // cache. Closing the background releases its cover and GPU resources.
        cache: false
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        visible: false
    }
    Loader {
        id: renderer
        anchors.fill: parent
        // Loading/error must not retain the previous cover's blur textures.
        active: surface.ready
        sourceComponent: Item {
            id: stage
            property real displayOpacity: 0
            function refresh(): void {
                texture.scheduleUpdate();
            }
            MultiEffect {
                id: blurred
                objectName: "playbackBackgroundBlur"
                width: surface.sampleWidth
                height: surface.sampleHeight
                source: cover
                visible: false
                blurEnabled: true
                blurMax: 64
                // MultiEffect blends discrete blur levels rather than scaling a
                // Gaussian radius linearly. Compensate the downsampled blend
                // to retain the original background edge softness.
                blur: 40 / 64 * Math.min(1, surface.surfaceScale * 1.25)
                autoPaddingEnabled: false
            }
            ShaderEffectSource {
                id: texture
                objectName: "playbackBackgroundTexture"
                width: surface.sampleWidth
                height: surface.sampleHeight
                sourceItem: blurred
                textureSize: surface.textureSize
                live: false
                visible: false
                // Image readiness creates the stage. Fade/time belong to the
                // final quad and never invalidate this static blurred texture.
                onTextureSizeChanged: scheduleUpdate()
            }
            ShaderEffect {
                objectName: "playbackBackgroundComposite"
                anchors.fill: parent
                property var source: texture
                property real time: surface.phase
                property real aspect: width / Math.max(1, height)
                property real rhythm: surface.rhythm ? 1 : 0
                property real fade: stage.displayOpacity
                property color baseColor: surface.baseColor
                property real backgroundOpacity: surface.backgroundOpacity
                fragmentShader: "qrc:/qt/qml/EchoMusic/Playback/shaders/lyric-fluid.frag.qsb"
            }
            NumberAnimation {
                target: stage
                property: "displayOpacity"
                from: 0
                to: 1
                duration: Theme.store.motionEnabled ? 800 : 0
                running: true
            }
        }
    }
}
