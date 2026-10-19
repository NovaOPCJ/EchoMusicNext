pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Theme

// Keep the intrinsic row centered inside the Control's available content box.
// Filling a short content box with RowLayout lets its minimum-height children
// overflow asymmetrically when padding/explicit height changes.
Item {
    id: content
    anchors.alignWhenCentered: false
    property string text: ""
    property string glyph: ""
    property bool loading: false
    property color color: Theme.text
    property color iconColor: color
    property font font: Theme.font(12, Theme.semiboldWeight)
    property real glyphSize: 16
    // Preserve explicit line boxes for icon and text rows beside tags.
    property real lineHeight: font.pixelSize * 1.5
    property real spacing: 8
    property int horizontalAlignment: Qt.AlignHCenter
    property bool glyphAfterText: false
    readonly property bool truncated: label.truncated
    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    RowLayout {
        id: row
        objectName: "buttonContentRow"
        x: content.horizontalAlignment === Qt.AlignLeft ? 0 : content.horizontalAlignment === Qt.AlignRight ? content.width - width : (content.width - width) / 2
        anchors.verticalCenter: parent.verticalCenter
        anchors.alignWhenCentered: false
        width: Math.max(0, Math.min(implicitWidth, content.width))
        height: implicitHeight
        spacing: content.spacing
        layoutDirection: content.glyphAfterText ? Qt.RightToLeft : Qt.LeftToRight
        Item {
            anchors.alignWhenCentered: false
            visible: content.loading
            Layout.preferredWidth: 16
            Layout.preferredHeight: 16
            Layout.fillHeight: true
            EchoSpinner {
                objectName: "buttonSpinner"
                anchors.centerIn: parent
                anchors.alignWhenCentered: false
                width: 16
                height: 16
                resourceActive: content.loading && content.visible
                color: content.color
            }
        }
        // RowLayout rounds direct-child positions to logical pixels. Keep its
        // sizing responsibility, but center the glyph inside the full-height
        // slot so half-pixel text line boxes retain the same visual center.
        Item {
            anchors.alignWhenCentered: false
            visible: content.glyph.length > 0
            Layout.preferredWidth: content.glyphSize
            Layout.preferredHeight: content.glyphSize
            Layout.fillHeight: true
            EchoIcon {
                objectName: "buttonGlyph"
                anchors.centerIn: parent
                anchors.alignWhenCentered: false
                width: content.glyphSize
                height: content.glyphSize
                name: content.glyph
                color: content.iconColor
            }
        }
        EchoTextTagRow {
            id: label
            objectName: "buttonLabel"
            visible: content.text.length > 0
            text: content.text
            color: content.color
            font: content.font
            lineHeight: content.lineHeight
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            fillTextWidth: true
        }
    }
}
