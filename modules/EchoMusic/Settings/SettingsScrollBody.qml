pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import QtQuick.Layouts
import EchoMusic.Controls

Flickable {
    id: scroll
    required property T.Popup surface
    default property alias rows: column.data
    implicitHeight: column.implicitHeight
    contentHeight: column.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ColumnLayout {
        id: column
        width: scroll.width - 24
        spacing: 16
    }
    ScrollBar.vertical: EchoPopupScrollBar {
        surface: scroll.surface
        view: scroll
    }
}
