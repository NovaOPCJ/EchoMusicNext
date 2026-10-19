pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Shapes
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Playback

Item {
    id: card
    required property var details
    objectName: "dspInstalledProviderCard"
    required property PlaybackAudioControls audio
    readonly property var manifest: details.manifest || {}
    readonly property var summary: ((manifest.resources || []).some(r => r.kind.toLowerCase() === "vpf" || (r.extensions || []).some(e => e.toLowerCase() === ".vpf")) ? [qsTr("支持 VPF")] : []).concat((manifest.presets || []).length > 0 ? [(manifest.presets || []).length + qsTr(" 个预设")] : []).concat(details.maxChannels > 0 ? [qsTr("最高 %1 声道").arg(details.maxChannels)] : [])
    implicitHeight: body.implicitHeight + 24
    Shape {
        anchors.fill: parent
        ShapePath {
            strokeWidth: 0
            fillGradient: LinearGradient {
                x1: 0
                y1: 0
                x2: card.width
                y2: card.height
                GradientStop {
                    position: 0
                    color: Theme.withAlpha(Theme.floating.accent, 0.11)
                }
                GradientStop {
                    position: 1
                    color: Theme.withAlpha(Theme.floating.accent, 0.03)
                }
            }
            PathRectangle {
                width: card.width
                height: card.height
                radius: Theme.cardRadius
            }
        }
    }
    ColumnLayout {
        id: body
        x: 12
        y: 12
        width: parent.width - 24
        spacing: 10
        RowLayout {
            Layout.fillHeight: false
            spacing: 5
            Layout.fillWidth: true
            EchoTextTagRow {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: card.details.name
                font: Theme.font(13, Theme.boldWeight)
                lineHeight: 20
                color: Theme.floating.text
                EchoTextTagRow {
                    text: "v" + card.details.version
                    font: Theme.font(9, Theme.semiboldWeight)
                    lineHeight: 20
                    color: Theme.floating.secondaryText
                }
            }
            RowLayout {
                Layout.fillHeight: false
                Layout.fillWidth: false
                Layout.minimumWidth: implicitWidth
                Layout.maximumWidth: implicitWidth
                spacing: 5
                Rectangle {
                    implicitWidth: 6
                    implicitHeight: 6
                    Layout.maximumWidth: 6
                    Layout.maximumHeight: 6
                    radius: 3
                    color: Theme.floating.secondaryText
                }
                EchoTextTagRow {
                    text: qsTr("已安装")
                    font: Theme.font(9, Theme.boldWeight)
                    lineHeight: 20
                    color: Theme.floating.secondaryText
                }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: card.details.description
            wrapMode: Text.Wrap
            font: Theme.font(10, Theme.regularWeight)
            color: Theme.floating.secondaryText
        }
        Flow {

            Layout.fillWidth: true
            visible: card.summary.length > 0
            spacing: 5
            Repeater {
                model: card.summary
                delegate: EchoTag {
                    required property string modelData
                    text: modelData
                    tone: EchoTag.Muted
                    colors: Theme.floating
                }
            }
        }
        RowLayout {
            Layout.fillHeight: false
            spacing: 5
            Layout.fillWidth: true
            Layout.topMargin: 2
            EchoTextTagRow {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: card.details.fileName
                font: Theme.font(10, Theme.regularWeight)
                lineHeight: 15
                color: Theme.floating.secondaryText
            }
            EchoButton {
                objectName: "activateInstalledDspProvider"
                Layout.fillWidth: false
                Layout.fillHeight: false
                Layout.maximumWidth: implicitWidth
                size: EchoControlSize.Compact
                accent: false
                colors: Theme.floating
                text: qsTr("开启")
                enabled: !card.audio.effectPending && !card.audio.providers.busy
                onClicked: card.audio.activateProvider(card.details.id)
            }
        }
    }
}
