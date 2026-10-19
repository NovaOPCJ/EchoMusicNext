pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme
import EchoMusic.Controls

Button {
    id: card
    required property string themeName
    required property string description
    required property bool selected
    signal chosen
    Accessible.name: themeName
    Accessible.role: Accessible.CheckBox
    Accessible.checked: selected
    padding: 10
    onClicked: chosen()
    background: Rectangle {
        radius: Theme.cardRadius
        color: Theme.mutedSurface
        border.width: card.visualFocus ? 2 : 1
        border.color: card.visualFocus ? Theme.accent : "transparent"
    }
    contentItem: ColumnLayout {
        spacing: 0
        ThemeThumbnail {
            radius: Theme.mediaRadius
            Layout.fillWidth: true
            solid: false
            seed: "#00cc65"
            EchoSelectionBadge {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                visible: card.selected
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.bottomMargin: 4
            text: card.themeName
            font: Theme.font(14, Theme.semiboldWeight)
            color: Theme.text
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            Layout.preferredHeight: 38.4
            text: card.description
            color: Theme.secondaryText
            font: Theme.font(12, Theme.regularWeight)
            lineHeight: 1.6
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
