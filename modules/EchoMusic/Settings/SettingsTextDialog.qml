pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Settings

EchoDialog {
    id: dialog
    property string text: ""
    property bool markdown: false
    preferredWidth: 620
    maximumHeight: 560
    windowHeightMargin: 140
    flushBody: true
    body: Flickable {
        id: scroll
        implicitHeight: content.implicitHeight
        contentHeight: content.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        SettingsMarkdownRenderer {
            id: renderer
        }
        TextEdit {
            id: content
            x: 0
            width: parent.width - 24
            text: dialog.markdown ? renderer.render(dialog.text, font, Theme.floatingText, Theme.floatingSecondaryText, Theme.floatingAccentText) : dialog.text
            textFormat: dialog.markdown ? TextEdit.RichText : TextEdit.PlainText
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            font: Theme.font(13, Theme.regularWeight)
            color: Theme.floatingSecondaryText
            selectionColor: Theme.floating.accent
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
        ScrollBar.vertical: EchoPopupScrollBar {
            surface: dialog
            view: scroll
        }
    }
    footerContent: RowLayout {
        Item {
            Layout.fillWidth: true
        }
        EchoSoftButton {
            text: qsTr("我知道了")
            accent: true
            colors: Theme.floating
            onClicked: dialog.close()
        }
    }
}
