pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme

Rectangle {
    id: notice
    property alias text: label.text
    Layout.fillWidth: true
    implicitHeight: label.implicitHeight + 16
    radius: Theme.controlRadius
    color: Theme.withAlpha(Theme.accent, .12)
    border.color: Theme.withAlpha(Theme.accent, .35)
    Label {
        id: label
        x: 12
        y: 8
        width: parent.width - 24
        font: Theme.font(12, Theme.semiboldWeight)
        color: Theme.floatingAccentText
        wrapMode: Text.Wrap
    }
}
