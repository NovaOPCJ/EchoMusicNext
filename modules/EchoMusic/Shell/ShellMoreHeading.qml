pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme

Item {
    id: heading
    required property string title
    property Component leadingControl: null
    property Component trailingControl: null
    // Header height includes a 28px action, 8px bottom padding and 1px border.
    implicitHeight: 37
    RowLayout {
        x: 8
        width: parent.width - 12
        height: 28
        spacing: 8
        Loader {
            visible: active
            active: heading.leadingControl !== null
            sourceComponent: heading.leadingControl
        }
        Label {
            text: heading.title
            color: Theme.floatingText
            font: Theme.font(13, Theme.semiboldWeight)
            Layout.fillWidth: true
        }
        Loader {
            visible: active
            active: heading.trailingControl !== null
            sourceComponent: heading.trailingControl
        }
    }
    Rectangle {
        width: parent.width
        height: 1
        y: parent.height - 1
        color: Theme.floating.subtleBorder
    }
}
