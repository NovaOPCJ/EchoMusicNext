pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme

// All composition/blur targets are bounded physical pixels. The final window
// quad only upsamples one finished texture; DPR never expands the blur graph.
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
        const ratio = Math.min(1, 192 / Math.max(1, width * renderDpr, height * renderDpr));
        return Qt.size(Math.max(1, Math.round(width * renderDpr * ratio)), Math.max(1, Math.round(height * renderDpr * ratio)));
    }
    readonly property real sampleWidth: textureSize.width / renderDpr
    readonly property real sampleHeight: textureSize.height / renderDpr
    readonly property int renderedFrames: renderer.item ? (renderer.item as RenderStage).submittedFrames : 0
    function refresh(): void {
        if (renderer.item)
            (renderer.item as RenderStage).requestFrame();
    }
    onTextureSizeChanged: refresh()
    onRhythmChanged: refresh()
    onPhaseChanged: if (rhythm)
        refresh()
    Image {
        id: cover
        objectName: "playbackBackgroundCover"
        width: 256 / surface.renderDpr
        height: width
        source: surface.coverSource
        sourceSize: Qt.size(256, 256)
        cache: false
        asynchronous: true
        visible: false
    }
    component RenderStage: Item {
        id: stage
        property real displayOpacity: 0
        property int submittedFrames: 0
        function requestFrame(): void {
            updateFrame.restart();
        }
        Component.onCompleted: requestFrame()
        // Coalesce size/cover/rhythm changes. Static, paused and hidden modes
        // never leave a live offscreen source running in the scene graph.
        Timer {
            id: updateFrame
            interval: 0
            onTriggered: {
                compositionTexture.scheduleUpdate();
                horizontalTexture.scheduleUpdate();
                texture.scheduleUpdate();
                ++stage.submittedFrames;
            }
        }
        ShaderEffect {
            id: composition
            objectName: "playbackBackgroundComposition"
            width: surface.sampleWidth
            height: surface.sampleHeight
            visible: false
            property var source: cover
            property real time: surface.phase
            property real aspect: surface.width / Math.max(1, surface.height)
            property real coverAspect: cover.implicitWidth / Math.max(1, cover.implicitHeight)
            property real rhythm: surface.rhythm ? 1 : 0
            fragmentShader: "qrc:/qt/qml/EchoMusic/Playback/shaders/lyric-fluid-compose.frag.qsb"
        }
        ShaderEffectSource {
            id: compositionTexture
            objectName: "playbackBackgroundCompositionTexture"
            width: surface.sampleWidth
            height: surface.sampleHeight
            sourceItem: composition
            textureSize: surface.textureSize
            format: ShaderEffectSource.RGBA16F
            live: false
            visible: false
        }
        ShaderEffect {
            id: horizontalBlur
            objectName: "playbackBackgroundHorizontalBlur"
            width: surface.sampleWidth
            height: surface.sampleHeight
            visible: false
            property var source: compositionTexture
            property vector2d step: Qt.vector2d(1 / surface.textureSize.width, 0)
            fragmentShader: "qrc:/qt/qml/EchoMusic/Playback/shaders/lyric-background-blur.frag.qsb"
        }
        ShaderEffectSource {
            id: horizontalTexture
            objectName: "playbackBackgroundHorizontalTexture"
            width: surface.sampleWidth
            height: surface.sampleHeight
            sourceItem: horizontalBlur
            textureSize: surface.textureSize
            format: ShaderEffectSource.RGBA16F
            live: false
            visible: false
        }
        ShaderEffect {
            id: verticalBlur
            objectName: "playbackBackgroundBlur"
            width: surface.sampleWidth
            height: surface.sampleHeight
            visible: false
            property var source: horizontalTexture
            property vector2d step: Qt.vector2d(0, 1 / surface.textureSize.height)
            fragmentShader: "qrc:/qt/qml/EchoMusic/Playback/shaders/lyric-background-blur.frag.qsb"
        }
        ShaderEffectSource {
            id: texture
            objectName: "playbackBackgroundTexture"
            width: surface.sampleWidth
            height: surface.sampleHeight
            sourceItem: verticalBlur
            textureSize: surface.textureSize
            format: ShaderEffectSource.RGBA16F
            live: false
            visible: false
        }
        ShaderEffect {
            objectName: "playbackBackgroundComposite"
            anchors.fill: parent
            property var source: texture
            property real fade: stage.displayOpacity
            property color baseColor: surface.baseColor
            property real backgroundOpacity: surface.backgroundOpacity
            property real rhythm: surface.rhythm ? 1 : 0
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
    Loader {
        id: renderer
        anchors.fill: parent
        // A failed/obsolete cover owns no residual effect textures.
        active: surface.ready
        sourceComponent: RenderStage {}
    }
}
