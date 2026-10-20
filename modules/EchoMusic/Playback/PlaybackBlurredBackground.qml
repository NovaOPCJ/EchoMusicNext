pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme
import EchoMusic.Playback

Item {
    id: surface
    required property url coverSource
    required property bool rhythm
    required property real phase
    required property color baseColor
    required property real backgroundOpacity
    property BackgroundArtwork currentArtwork: null
    property BackgroundArtwork incomingArtwork: null
    readonly property BackgroundArtwork activeArtwork: incomingArtwork?.ready ? incomingArtwork : currentArtwork
    readonly property bool ready: activeArtwork !== null && activeArtwork.ready
    readonly property real renderDpr: Math.max(1, Screen.devicePixelRatio)
    readonly property size textureSize: {
        const ratio = Math.min(1, 192 / Math.max(1, width * renderDpr, height * renderDpr));
        return Qt.size(Math.max(1, Math.round(width * renderDpr * ratio)), Math.max(1, Math.round(height * renderDpr * ratio)));
    }
    property int renderedFrames: 0
    property real transition: 0
    property bool initialized: false
    function refresh(): void {
        currentArtwork?.refresh();
        incomingArtwork?.refresh();
    }
    function finishTransition(): void {
        if (!incomingArtwork)
            return;
        const old = currentArtwork;
        currentArtwork = incomingArtwork;
        incomingArtwork = null;
        transition = 0;
        old?.destroy();
    }
    function replaceCover(): void {
        if (!initialized)
            return;
        fade.stop();
        if (incomingArtwork) {
            // Keep at most two cover graphs, even during rapid queue changes.
            if (incomingArtwork.ready && transition >= 0.5)
                finishTransition();
            else {
                incomingArtwork.destroy();
                incomingArtwork = null;
            }
        }
        transition = 0;
        incomingArtwork = artworkRecipe.createObject(surface, {
            coverSource: surface.coverSource,
            phase: Qt.binding(() => surface.phase),
            targetSize: Qt.binding(() => surface.textureSize),
            renderDpr: Qt.binding(() => surface.renderDpr)
        });
    }
    onCoverSourceChanged: replaceCover()
    Component.onCompleted: {
        initialized = true;
        replaceCover();
    }
    Component {
        id: artworkRecipe
        BackgroundArtwork {
            id: artwork
            visible: false
            onRendered: if (surface.activeArtwork === artwork)
                ++surface.renderedFrames
            onPrepared: if (surface.incomingArtwork === artwork) {
                if (Theme.store.motionEnabled)
                    fade.restart();
                else
                    surface.finishTransition();
            }
            onFailedChanged: if (failed && surface.incomingArtwork === artwork) {
                fade.stop();
                surface.currentArtwork?.destroy();
                surface.currentArtwork = null;
                // Keep the failed Image status observable, but no effect chain.
            }
        }
    }
    Connections {
        target: Theme.store
        function onMotionEnabledChanged(): void {
            if (!Theme.store.motionEnabled) {
                fade.stop();
                surface.finishTransition();
            }
        }
    }
    NumberAnimation {
        id: fade
        target: surface
        property: "transition"
        from: 0
        to: 1
        duration: 500
        easing.type: Easing.InOutSine
        onFinished: surface.finishTransition()
    }
    Loader {
        anchors.fill: parent
        active: surface.ready
        sourceComponent: ShaderEffect {
            property var source: surface.activeArtwork?.texture ?? null
            property var previous: surface.currentArtwork?.texture ?? surface.activeArtwork?.texture ?? null
            property real blend: surface.incomingArtwork?.ready ? surface.transition : 1
            property real hasPrevious: surface.currentArtwork ? 1 : 0
            property color baseColor: surface.baseColor
            property real backgroundOpacity: surface.backgroundOpacity
            fragmentShader: "qrc:/qt/qml/EchoMusic/Playback/shaders/lyric-fluid.frag.qsb"
        }
    }
}
