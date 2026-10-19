pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Settings
import EchoMusic.Theme

EchoDialog {
    id: dialog
    objectName: "settingsChangelogDialog"
    property string markdown: ""
    heading: qsTr("更新日志")
    preferredWidth: 520
    windowHeightMargin: 140
    flushBody: true
    body: Item {
        implicitHeight: Math.min(288, dialog.parent ? dialog.parent.height / dialog.uiScale * 0.4 : 288, notes.contentHeight)
        Rectangle {
            anchors.fill: parent
            anchors.rightMargin: 24
            radius: Theme.cardRadius
            color: Theme.floating.mutedSurface
        }
        Flickable {
            id: notes
            anchors.fill: parent
            contentHeight: text.implicitHeight + 24
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            SettingsMarkdownRenderer {
                id: renderer
            }
            TextEdit {
                id: text
                objectName: "settingsChangelogText"
                x: 16
                y: 12
                width: parent.width - 24 - 32
                text: renderer.render(dialog.markdown, font, Theme.floatingText, Theme.floatingSecondaryText, Theme.floatingAccentText)
                font: Theme.font(14, Theme.regularWeight)
                textFormat: TextEdit.RichText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                selectionColor: Theme.floating.accent
                onLinkActivated: link => Qt.openUrlExternally(link)
            }
            ScrollBar.vertical: EchoPopupScrollBar {
                surface: dialog
                view: notes
            }
        }
    }
    footerContent: RowLayout {
        Item {
            Layout.fillWidth: true
        }
        EchoSoftButton {
            text: qsTr("关闭")
            colors: Theme.floating
            onClicked: dialog.close()
        }
    }
}
