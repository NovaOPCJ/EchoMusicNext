pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Rectangle {
    id: row
    objectName: "profileDeviceRow"
    required property string title
    required property string detail
    required property string activity
    required property bool currentDevice
    required property bool newDevice
    required property string deviceKey
    required property bool canRemove
    property bool removing: false
    property bool removalBusy: false
    signal removalRequested(string key)
    // Use a 76px minimum height with 12px padding inside the border.
    // Visible text lines determine the natural height.
    implicitHeight: Math.max(76, content.implicitHeight + 26)
    height: implicitHeight
    radius: Theme.cardRadius
    color: Theme.floating.mutedSurface
    border.color: Theme.floating.subtleBorder
    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 13
        spacing: 12
        Rectangle {
            objectName: "profileDeviceIconBox"
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            radius: Theme.cardRadius
            color: row.currentDevice ? Theme.withAlpha(Theme.accent, 0.15) : Theme.floating.hover
            EchoIcon {
                anchors.centerIn: parent
                width: 20
                height: 20
                name: "device-mobile"
                color: row.currentDevice ? Theme.floatingAccentText : Theme.floatingText
            }
        }
        ColumnLayout {
            Layout.fillHeight: false
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 0
            EchoTextTagRow {
                objectName: "profileDeviceTitle"
                Layout.fillHeight: false
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: row.title
                font: Theme.font(13, Theme.blackWeight)
                color: Theme.floatingText
                spacing: 8
                EchoTag {
                    objectName: "profileDeviceStatus"
                    visible: row.currentDevice || row.newDevice
                    text: row.currentDevice ? qsTr("本机") : qsTr("新设备")
                    tone: row.currentDevice ? EchoTag.Accent : EchoTag.Custom
                    customTone: Theme.floating.success
                    colors: Theme.floating
                }
            }
            Label {
                visible: text.length > 0
                Layout.fillWidth: true
                text: row.detail
                elide: Text.ElideRight
                font: Theme.font(11, Theme.boldWeight)
                lineHeightMode: Text.FixedHeight
                lineHeight: 16.5
                color: Theme.floatingSecondaryText
            }
            Label {
                visible: text.length > 0
                Layout.fillWidth: true
                text: row.activity
                elide: Text.ElideRight
                font: Theme.font(11, Theme.boldWeight)
                lineHeightMode: Text.FixedHeight
                lineHeight: 16.5
                color: Theme.floatingSecondaryText
            }
        }
        EchoSoftButton {
            objectName: "profileDeviceRemove"
            visible: !row.currentDevice
            text: qsTr("移除")
            size: EchoControlSize.Standard
            enabled: row.canRemove && !row.removalBusy
            colors: Theme.floating
            danger: true
            contentItem: EchoButtonContent {
                text: qsTr("移除")
                font: Theme.font(12, Theme.semiboldWeight)
                color: Theme.floating.dangerText
                loading: row.removing
            }
            onClicked: row.removalRequested(row.deviceKey)
        }
    }
}
