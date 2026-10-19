pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import EchoMusic.Theme

Item {
    id: root
    property bool playing: false
    property color color: Theme.text
    property real playOffset: 0
    property real phase: playing ? 1 : 0
    property bool initialized: false
    readonly property bool ready: playIcon.status === Image.Ready && pauseIcon.status === Image.Ready
    readonly property bool canAnimate: visible && ready && Theme.store.motionEnabled && Window.window !== null && Window.window.visible && Window.window.visibility !== Window.Minimized
    readonly property bool transitioning: transition.running
    implicitWidth: 20
    implicitHeight: 20
    objectName: "playPauseIcon"
    // Only presentation interpolates; the playback command and accessible name
    // change immediately. A reversal starts at the current phase, without a queue.
    function synchronize() {
        if (!initialized)
            return;
        transition.stop();
        if (canAnimate) {
            transition.from = phase;
            transition.to = playing ? 1 : 0;
            transition.start();
        } else {
            phase = playing ? 1 : 0;
        }
    }
    Component.onCompleted: {
        phase = playing ? 1 : 0;
        initialized = true;
    }
    onPlayingChanged: synchronize()
    onCanAnimateChanged: if (initialized && !canAnimate) {
        transition.stop();
        phase = playing ? 1 : 0;
    }
    EchoIcon {
        id: playIcon
        objectName: "playPausePlayIcon"
        anchors.fill: parent
        // anchors.fill cannot carry an offset, so translate only the glyph.
        transform: Translate {
            x: root.playOffset
        }
        name: "play"
        color: root.color
        opacity: 1 - root.phase
        scale: 1 - 0.18 * root.phase
    }
    EchoIcon {
        id: pauseIcon
        objectName: "playPausePauseIcon"
        anchors.fill: parent
        name: "pause"
        color: root.color
        opacity: root.phase
        scale: 0.82 + 0.18 * root.phase
    }
    NumberAnimation {
        id: transition
        target: root
        property: "phase"
        duration: 180
        easing.type: Easing.OutCubic
    }
}
