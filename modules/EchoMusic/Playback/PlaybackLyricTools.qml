pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Playback

// Native Columns measure visible groups. The reset action retains its slot.
// All controls use the same lyric service across playback surfaces.
Flickable {
    id: tools
    EchoWheelScroll.enabled: true
    EchoWheelScroll.motionEnabled: Theme.store.motionEnabled
    objectName: "playbackLyricTools"
    required property PlaybackService playback
    required property ThemeColors colors
    signal sourceRequested
    width: 36
    contentWidth: width
    contentHeight: groups.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick
    Column {
        id: groups
        width: tools.width
        spacing: 24
        Column {
            spacing: 8
            EchoSoftButton {
                id: chooseSource
                objectName: "lyricSourceAction"
                implicitWidth: 36
                implicitHeight: 36
                leftPadding: 8
                rightPadding: 8
                colors: tools.colors
                glyphSize: 14
                glyph: "list"
                text: ""
                Accessible.name: qsTr("选择歌词")
                EchoToolTip {
                    parent: chooseSource
                    text: chooseSource.Accessible.name
                    requestedVisible: chooseSource.hovered
                }
                onClicked: tools.sourceRequested()
            }
            Repeater {
                model: [-1, 1, 0]
                EchoSoftButton {
                    id: offsetAction
                    objectName: "lyricOffsetAction-" + modelData
                    enabled: opacity > 0
                    required property int modelData
                    implicitWidth: 36
                    implicitHeight: 36
                    leftPadding: 8
                    rightPadding: 8
                    colors: tools.colors
                    glyphSize: modelData === 0 ? 14 : 15
                    glyph: modelData < 0 ? "rotate-2" : modelData > 0 ? "rotate-clockwise-2" : "refresh"
                    text: ""
                    visible: tools.playback.lyrics.count > 0
                    opacity: modelData !== 0 || tools.playback.lyricText.trackOffsetMs !== 0 ? 1 : 0
                    Accessible.name: modelData < 0 ? qsTr("歌词后退 %1 秒").arg(tools.playback.lyricText.offsetStep) : modelData > 0 ? qsTr("歌词前进 %1 秒").arg(tools.playback.lyricText.offsetStep) : qsTr("重置单曲偏移")
                    EchoToolTip {
                        parent: offsetAction
                        text: offsetAction.Accessible.name
                        requestedVisible: offsetAction.hovered
                    }
                    onClicked: modelData === 0 ? tools.playback.lyricText.resetTrackOffset() : tools.playback.lyricText.adjustTrackOffset(modelData)
                }
            }
        }
        Column {
            spacing: 8
            visible: tools.playback.lyrics.count > 0
            Repeater {
                model: ["translation", "romanization", "copy"]
                EchoSoftButton {
                    id: action
                    objectName: "lyricTextAction-" + modelData
                    required property string modelData
                    implicitWidth: 36
                    implicitHeight: 36
                    leftPadding: 8
                    rightPadding: 8
                    colors: tools.colors
                    glyphSize: 14
                    visible: modelData === "copy" || (modelData === "translation" ? tools.playback.lyrics.hasTranslation : tools.playback.lyrics.hasRomanization)
                    text: modelData === "translation" ? qsTr("译") : modelData === "romanization" ? qsTr("音") : ""
                    glyph: modelData === "copy" ? "copy" : ""
                    font: Theme.font(12, Theme.boldWeight)
                    accent: modelData === "translation" ? tools.playback.lyricText.wantTranslation : modelData === "romanization" && tools.playback.lyricText.wantRomanization
                    Accessible.name: modelData === "translation" ? qsTr("翻译") : modelData === "romanization" ? qsTr("音译") : qsTr("复制歌词")
                    Accessible.checkable: modelData !== "copy"
                    Accessible.checked: accent
                    onClicked: {
                        if (modelData === "translation")
                            tools.playback.lyricText.wantTranslation = !tools.playback.lyricText.wantTranslation;
                        else if (modelData === "romanization")
                            tools.playback.lyricText.wantRomanization = !tools.playback.lyricText.wantRomanization;
                        else
                            tools.playback.lyricText.copyLyrics();
                    }
                }
            }
        }
    }
}
