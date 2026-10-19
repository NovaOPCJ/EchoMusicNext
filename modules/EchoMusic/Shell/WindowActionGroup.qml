pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A caption's whole rectangle is an input region, including disabled actions
// and inter-button gaps; those presses must never reach the window drag layer.
Control {
    default property alias buttons: row.data
    Layout.fillWidth: false
    Layout.fillHeight: false
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    padding: 0
    background: MouseArea {
        objectName: "windowActionGroupInputBlocker"
        onDoubleClicked: mouse => mouse.accepted = true
    }
    contentItem: RowLayout {
        id: row
        spacing: 4
    }
}
