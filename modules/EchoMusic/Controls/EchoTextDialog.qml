pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Controls
import EchoMusic.Theme

// Long introductions belong in a bounded, scroll-owning body, not the short
// description above EchoDialog's body. Keep its heading/close action stationary.
EchoDialog {
    id: dialog
    property string text: ""
    preferredWidth: 520
    flushBody: true
    body: Flickable {
        id: scroll
        objectName: "textDialogScroll"
        implicitHeight: contentHeight
        contentWidth: width
        contentHeight: label.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        focus: true
        Keys.onPressed: event => {
            switch (event.key) {
            case Qt.Key_Home:
                scroll.contentY = scroll.originY;
                break;
            case Qt.Key_End:
                scroll.contentY = scroll.originY + Math.max(0, scroll.contentHeight - scroll.height);
                break;
            case Qt.Key_Down:
                scroll.EchoWheelScroll.scrollBy(40);
                break;
            case Qt.Key_Up:
                scroll.EchoWheelScroll.scrollBy(-40);
                break;
            case Qt.Key_PageDown:
                scroll.EchoWheelScroll.scrollBy(scroll.height * .9);
                break;
            case Qt.Key_PageUp:
                scroll.EchoWheelScroll.scrollBy(-scroll.height * .9);
                break;
            default:
                return;
            }
            event.accepted = true;
        }
        Label {
            id: label
            objectName: "textDialogContent"
            width: Math.max(0, scroll.width - 24)
            text: dialog.text
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: Theme.floatingSecondaryText
            font: Theme.font(dialog.descriptionPixelSize, Theme.regularWeight)
        }
        ScrollBar.vertical: EchoPopupScrollBar {
            surface: dialog
            view: scroll
        }
    }
}
