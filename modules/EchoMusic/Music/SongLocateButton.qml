pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls

// Always-filled neutral locate action with shared pointer and keyboard states.
EchoIconButton {
    id: button
    implicitWidth: 36
    implicitHeight: 36
    padding: 9
    glyph: "current-location"
    text: qsTr("定位当前播放")
    tooltipText: text
    foreground: colors.text
    background: EchoActionBackground {
        objectName: "songLocateBackground"
        colors: button.colors
        pressed: button.down
        highlighted: button.enabled && (button.hovered || button.visualFocus)
        border.width: button.visualFocus ? 2 : 0
        border.color: button.colors.accent
    }
}
