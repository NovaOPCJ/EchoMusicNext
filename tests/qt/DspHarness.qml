pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Controls
import EchoMusic.Playback
import EchoMusic.Theme

ApplicationWindow {
    id: host
    required property PlaybackAudioControls audio
    width: 900
    height: 680
    visible: true
    color: Theme.main
    readonly property alias manager: manager
    EchoBackdropRoot {
        anchors.fill: parent
        Rectangle {
            anchors.fill: parent
            color: Theme.main
        }
        DspProviderPanel {
            objectName: "productionDspPanel"
            audio: host.audio
            visible: host.audio.providerId.length > 0
            width: 340
            height: 430
            anchors.centerIn: parent
        }
    }
    DspProviderDialog {
        id: manager
        audio: host.audio
    }
}
