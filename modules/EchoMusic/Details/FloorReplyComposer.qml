pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Rectangle {
    id: composer
    required property CommentsController controller
    required property var target
    objectName: "floorReplyComposer"
    implicitHeight: content.implicitHeight + 28
    radius: Theme.cardRadius
    color: Theme.mutedSurface
    border.width: 1
    border.color: Theme.subtleBorder
    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            EchoTextTagRow {
                Layout.fillWidth: true
                text: qsTr("回复 %1").arg(composer.target.user_name || "")
                font: Theme.font(12, Theme.semiboldWeight)
                color: Theme.secondaryText
            }
            EchoIconButton {
                objectName: "cancelFloorReply"
                glyph: "x"
                text: qsTr("取消回复")
                implicitWidth: 32
                implicitHeight: 32
                padding: 8
                enabled: !composer.controller.busy
                onClicked: composer.controller.closeReply()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Rectangle {
                Layout.preferredWidth: 2
                Layout.fillHeight: true
                color: Theme.border
            }
            Label {
                Layout.fillWidth: true
                text: composer.target.content || ""
                elide: Text.ElideRight
                color: Theme.secondaryText
                font: Theme.font(12, Theme.regularWeight)
            }
        }
        EchoTextArea {
            id: draft
            objectName: "replyDraft"
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            text: composer.controller.replyDraft
            enabled: !composer.controller.busy
            placeholderText: composer.controller.signedIn ? qsTr("写下你的回复…") : qsTr("登录后即可回复")
            Accessible.name: qsTr("回复 %1").arg(composer.target.user_name || "")
            Accessible.description: qsTr("Enter 发送，Shift + Enter 换行")
            onTextChanged: composer.controller.replyDraft = text
            Keys.onReturnPressed: event => {
                if (inputMethodComposing || event.modifiers !== Qt.NoModifier)
                    event.accepted = false;
                else {
                    event.accepted = true;
                    if (!event.isAutoRepeat)
                        composer.controller.submitReply();
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: Array.from(draft.text).length + " / 200"
                font: Theme.font(11, Theme.regularWeight)
                color: Theme.secondaryText
            }
            EchoSoftButton {
                objectName: "replySend"
                text: composer.controller.busy ? qsTr("发送中…") : qsTr("发送回复")
                enabled: composer.controller.signedIn && !composer.controller.busy && draft.text.trim().length > 0 && Array.from(draft.text).length <= 200
                onClicked: composer.controller.submitReply()
            }
        }
    }
}
