pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls

// Shared neutral sort action. Opens the order editor in tracks mode.
EchoIconButton {
    id: button
    implicitWidth: 36
    implicitHeight: 36
    padding: 9
    glyph: "arrows-sort"
    text: qsTr("调整歌曲顺序")
    tooltipText: text
    foreground: colors.text
    background: EchoActionBackground {
        objectName: "songSortBackground"
        colors: button.colors
        pressed: button.down
        highlighted: button.enabled && (button.hovered || button.visualFocus)
        border.width: button.visualFocus ? 2 : 0
        border.color: button.colors.accent
    }
}
