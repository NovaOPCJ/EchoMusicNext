pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Theme

Rectangle {
    id: card
    default property alias content: layout.data
    property real spacing: 12
    implicitHeight: layout.implicitHeight + 36
    color: Theme.floating.mutedSurface
    border.color: Theme.floatingBorder
    radius: Theme.cardRadius
    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 18
        spacing: card.spacing
    }
}
