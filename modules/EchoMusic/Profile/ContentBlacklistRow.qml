pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Account
import EchoMusic.Controls
import EchoMusic.Theme

Rectangle {
    id: row
    objectName: "contentBlacklistRow"
    required property ContentBlacklistService service
    required property string entryKey
    required property string entryName
    required property string createdDate
    property bool resourceActive: visible
    required property bool removable
    readonly property string displayName: entryName.trim().length ? entryName.trim() : service.singer ? qsTr("未命名歌手") : qsTr("未命名歌曲")
    height: 56
    radius: Theme.controlRadius
    color: hover.hovered ? Theme.floating.hover : "transparent"
    HoverHandler {
        id: hover
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 7
        anchors.bottomMargin: 7
        spacing: 10
        Rectangle {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: Theme.controlRadius
            color: Theme.floating.mutedSurface
            EchoIcon {
                anchors.centerIn: parent
                width: 17
                height: 17
                name: row.service.singer ? "eye-off" : "heart-off"
                color: Theme.floating.iconMain
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.fillHeight: false
            spacing: 2
            EchoTextTagRow {
                id: title
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: row.displayName
                font: Theme.font(12, Theme.blackWeight)
                color: Theme.floatingText
                EchoToolTip {
                    parent: title
                    text: row.displayName
                    requestedVisible: hover.hovered && title.truncated
                }
            }
            Label {
                visible: row.createdDate.length > 0
                Layout.fillWidth: true
                text: (row.service.singer ? qsTr("屏蔽于 ") : qsTr("标记于 ")) + row.createdDate
                font: Theme.font(10, Theme.mediumWeight)
                lineHeightMode: Text.FixedHeight
                lineHeight: 15
                color: Theme.floatingSecondaryText
                elide: Text.ElideRight
            }
        }
        EchoIconButton {
            id: undo
            objectName: "contentBlacklistUndo"
            implicitWidth: 32
            implicitHeight: 32
            padding: 8
            glyph: row.service.removing && row.service.removingKey === row.entryKey ? "refresh" : "rotate-2"
            text: (row.service.singer ? qsTr("取消屏蔽 ") : qsTr("撤销不感兴趣 ")) + row.displayName
            tooltipText: text
            colors: Theme.floating
            enabled: row.removable && !row.service.loading && !row.service.removing
            contentItem: EchoIcon {
                name: undo.glyph
                color: undo.foreground
                opacity: undo.enabled ? 1 : undo.disabledOpacity
                RotationAnimator on rotation {
                    from: 0
                    to: 360
                    duration: 1000
                    loops: Animation.Infinite
                    running: row.resourceActive && row.visible && row.service.removing && row.service.removingKey === row.entryKey && Theme.store.motionEnabled && row.Window.window !== null && row.Window.window.visible && row.Window.window.visibility !== Window.Minimized
                }
            }
            onClicked: row.service.requestRemoval(row.entryKey)
        }
    }
}
