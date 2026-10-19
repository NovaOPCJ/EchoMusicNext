pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme

// A 1px subtle divider with 4px vertical and 6px horizontal margins.
MenuSeparator {
    // Invisible separators remain in Qt's contentModel. Collapse their geometry
    // as well, so the measured menu and the ListView agree on the scroll range.
    implicitHeight: visible ? topPadding + contentItem.implicitHeight + bottomPadding : 0
    horizontalPadding: 6
    verticalPadding: 4
    contentItem: Item {
        implicitHeight: 1
        Rectangle {
            anchors.fill: parent
            color: Theme.floating.subtleBorder
        }
    }
}
