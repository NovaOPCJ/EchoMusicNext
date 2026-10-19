pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme

// Profile action button with shared geometry and interaction states.
EchoIconButton {
    id: button
    property bool danger: false
    implicitWidth: 36
    implicitHeight: 36
    padding: 8
    scale: down && enabled ? 0.9 : 1
    Behavior on scale {
        NumberAnimation {
            duration: Theme.fastDuration
            easing.type: Easing.OutCubic
        }
    }
    foreground: danger && hovered ? Theme.colors.danger : hovered || visualFocus ? Theme.text : Theme.iconMain
    background: Rectangle {
        radius: Theme.controlRadius
        color: button.hovered ? button.danger ? Theme.withAlpha(Theme.colors.danger, 0.1) : Theme.hover : "transparent"
        border.width: button.visualFocus ? 2 : 1
        border.color: button.visualFocus ? Theme.accent : Theme.colors.controlBorder
    }
}
