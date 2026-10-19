pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme

TextArea {
    id: control
    property ThemeColors colors: Theme.colors
    implicitHeight: 120
    padding: 10
    leftPadding: 12
    rightPadding: 12
    wrapMode: TextEdit.Wrap
    font: Theme.font(13, Theme.mediumWeight)
    color: colors.text
    placeholderTextColor: colors.secondaryText
    selectionColor: colors.accent
    selectedTextColor: "#000000"
    background: Rectangle {
        radius: Theme.controlRadius
        color: Theme.withAlpha(control.colors.tone, 0.05)
        border.width: 1
        border.color: control.activeFocus ? Theme.withAlpha(control.colors.accent, 0.35) : control.colors.border
    }
}
