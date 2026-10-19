pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme

Slider {
    id: control
    required property string label
    required property string status
    from: 0
    to: 100
    stepSize: 1
    live: true
    implicitHeight: 48
    padding: 0
    Accessible.name: label + qsTr("推荐强度")
    Accessible.description: status + "，" + Math.round(value)
    handle: Item {}
    background: Rectangle {
        radius: Theme.cardRadius
        color: Theme.floating.mutedSurface
        border.color: control.hovered ? Theme.floatingText : Theme.floatingBorder
        Rectangle {
            width: parent.width * control.position
            height: parent.height
            radius: Theme.cardRadius
            color: Theme.withAlpha(Theme.accent, 0.12)
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10
            Label {
                Layout.fillWidth: true
                text: control.label
                elide: Text.ElideRight
                font: Theme.font(13, Theme.extraBoldWeight)
                color: Theme.floatingText
            }
            Label {
                text: control.status + (control.value === 50 ? "" : " · " + Math.round(control.value))
                font: Theme.font(11, Theme.regularWeight)
                color: control.value === 0 ? Theme.floatingText : Theme.floatingSecondaryText
            }
        }
    }
    opacity: enabled ? 1 : 0.5
}
