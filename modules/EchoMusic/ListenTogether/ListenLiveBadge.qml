pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Rectangle {
    id: badge
    property string kind: "card"
    property string text: kind === "session" ? qsTr("同步中") : "LIVE"
    readonly property bool pill: kind !== "hero"
    readonly property real insetX: kind === "session" ? 10 : pill ? 9 : 0
    readonly property real insetY: kind === "session" ? 8 : pill ? 7 : 0
    implicitWidth: contents.implicitWidth + insetX * 2
    implicitHeight: contents.implicitHeight + insetY * 2
    color: pill ? "#c7052e16" : "transparent"
    radius: height / 2
    RowLayout {
        id: contents
        anchors.centerIn: parent
        spacing: badge.pill ? 7 : 8
        Item {
            Layout.preferredWidth: badge.kind === "card" ? 5 : 7
            Layout.preferredHeight: badge.kind === "card" ? 5 : 7
            Rectangle {
                visible: badge.kind !== "card"
                anchors.centerIn: parent
                width: parent.width + 10
                height: width
                radius: width / 2
                color: "#2922c55e"
            }
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "#22c55e"
            }
        }
        EchoTextTagRow {
            text: badge.text
            color: badge.pill ? "#d1fae5" : Theme.secondaryText
            font: Theme.spacedFont(badge.pill ? 9 : 12, badge.pill ? 950 : 800, badge.pill ? 0.8 : 0)
            lineHeight: badge.pill ? 9 : 18
        }
    }
}
