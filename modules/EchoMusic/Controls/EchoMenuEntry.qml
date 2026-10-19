pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme

// Shared wrapping context-menu row. EchoMenu uses it for Action entries;
// it can also be declared directly for custom visibility or danger state.
MenuItem {
    id: entry
    property bool danger: false
    // Action-driven rows read the same flag from the Action's dynamic property.
    // Action carries `danger` as a dynamic property; subscript access avoids
    // declaring every Action subclass used by callers.
    readonly property bool actionDanger: action !== null && action !== undefined && action["danger"] === true
    readonly property color labelColor: entry.danger || entry.actionDanger ? Theme.floating.dangerText : Theme.floatingText
    font: Theme.font(13, Theme.semiboldWeight)
    leftPadding: 10
    rightPadding: 10
    topPadding: 8
    bottomPadding: 8
    implicitWidth: Math.max(168, measure.advanceWidth + entry.leftPadding + entry.rightPadding + (entry.checkable ? 22 : 0))
    // Hidden rows collapse inside the ListView.
    implicitHeight: entry.visible ? Math.max(36, body.implicitHeight + entry.topPadding + entry.bottomPadding) : 0
    TextMetrics {
        id: measure
        font: entry.font
        text: entry.text
    }
    indicator: EchoIcon {
        objectName: "menuEntryCheck"
        x: 12
        y: entry.topPadding + (entry.availableHeight - height) / 2
        width: 16
        height: 16
        visible: entry.checked
        name: "check"
        color: entry.labelColor
    }
    contentItem: Text {
        id: body
        x: entry.checkable ? 22 : 0
        width: Math.max(0, entry.availableWidth - (entry.checkable ? 22 : 0))
        anchors.verticalCenter: parent.verticalCenter
        text: entry.text
        textFormat: Text.PlainText
        font: entry.font
        color: entry.labelColor
        opacity: entry.enabled ? 1 : 0.5
        wrapMode: Text.Wrap
        lineHeight: 20
        lineHeightMode: Text.FixedHeight
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: Theme.controlRadius
        color: !entry.enabled ? "transparent" : entry.danger || entry.actionDanger ? (entry.down ? Theme.withAlpha(Theme.floating.danger, 0.16) : entry.highlighted ? Theme.withAlpha(Theme.floating.danger, 0.12) : "transparent") : entry.down ? Theme.floating.neutralPressed : entry.highlighted ? Theme.floating.hover : "transparent"
    }
}
