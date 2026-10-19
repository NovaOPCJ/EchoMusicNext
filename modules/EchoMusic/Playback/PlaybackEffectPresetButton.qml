pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme

// Equalizer preset action with shared states and explicit compact typography.
EchoSoftButton {
    id: button
    property real minimumHeight: 38
    colors: Theme.floating
    font: Theme.font(13, Theme.semiboldWeight)
    leftPadding: 10
    rightPadding: 10
    implicitHeight: Math.max(minimumHeight, label.implicitHeight + 16)
    contentItem: Item {
        implicitWidth: label.implicitWidth
        implicitHeight: label.implicitHeight
        EchoTextTagRow {
            id: label
            objectName: "effectPresetLabel"
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width)
            text: button.text
            font: button.font
            color: button.foreground
            horizontalAlignment: Text.AlignHCenter
            wrap: true
            lineHeight: 20
        }
    }
}
