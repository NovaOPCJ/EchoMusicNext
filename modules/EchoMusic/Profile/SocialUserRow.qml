pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Account
import EchoMusic.Controls
import EchoMusic.Theme

Item {
    id: row
    objectName: "profileSocialRow"
    required property string entryKey
    required property double userId
    property double currentUserId: 0
    property bool pending: false
    property bool listLoading: false
    signal followRequested
    signal messageRequested
    required property string nickname
    required property string avatarUrl
    required property url avatarIdentityIcon
    required property string avatarIdentityLabel
    required property var identityBadges
    required property string description
    required property string meta
    required property bool following
    required property bool canUnfollow
    required property bool relationshipKnown
    property bool resourceActive: true
    property real horizontalInset: 0
    height: Math.max(64, content.implicitHeight + 20)
    Rectangle {
        objectName: "profileSocialCard"
        anchors.fill: parent
        anchors.leftMargin: row.horizontalInset
        anchors.rightMargin: row.horizontalInset
        radius: Theme.cardRadius
        color: Theme.floating.mutedSurface
        border.color: Theme.floatingBorder
        RowLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12
            AccountAvatar {
                objectName: "socialAccountAvatar"
                Layout.preferredWidth: 42
                Layout.preferredHeight: 42
                source: row.resourceActive ? row.avatarUrl : ""
                identityIcon: row.avatarIdentityIcon
                identityLabel: row.avatarIdentityLabel
                resourceActive: row.resourceActive
                colors: Theme.floating
            }
            ColumnLayout {
                Layout.fillHeight: false
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 3
                RowLayout {
                    Layout.fillHeight: false
                    Layout.fillWidth: true
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        text: row.nickname
                        elide: Text.ElideRight
                        color: Theme.floatingText
                        font: Theme.font(13, Theme.blackWeight)
                    }
                    Label {
                        // Singer identity is already represented by its shared tag.
                        visible: row.meta.length > 0 && row.meta.trim() !== qsTr("歌手")
                        text: row.meta
                        elide: Text.ElideRight
                        Layout.maximumWidth: 100
                        color: Theme.floatingSecondaryText
                        font: Theme.font(10, Theme.boldWeight)
                    }
                }
                UserIdentityBadges {
                    Layout.fillWidth: true
                    Layout.topMargin: 3
                    entries: row.identityBadges
                    colors: Theme.floating
                }
                Label {
                    Layout.fillWidth: true
                    visible: row.description.length > 0 && row.description.trim() !== qsTr("歌手")
                    text: row.description
                    elide: Text.ElideRight
                    color: Theme.floatingSecondaryText
                    font: Theme.font(11, Theme.boldWeight)
                }
            }
            RowLayout {
                Layout.fillWidth: false
                Layout.fillHeight: false
                spacing: 6
                EchoSoftButton {
                    objectName: "socialFollowButton"
                    visible: row.relationshipKnown && row.userId !== row.currentUserId
                    implicitWidth: 96
                    accent: !row.following
                    loading: row.pending
                    text: row.following ? qsTr("取消关注") : qsTr("关注")
                    glyph: row.following ? "minus" : "plus"
                    glyphSize: 14
                    size: EchoControlSize.Standard
                    font: Theme.font(12, Theme.boldWeight)
                    colors: Theme.floating
                    enabled: !row.listLoading && !row.pending
                    onClicked: row.followRequested()
                }
                EchoSoftButton {
                    objectName: "socialMessageButton"
                    text: qsTr("私信")
                    glyph: "message-circle"
                    glyphSize: 14
                    size: EchoControlSize.Standard
                    accent: false
                    colors: Theme.floating
                    enabled: row.userId > 0 && row.userId !== row.currentUserId
                    onClicked: row.messageRequested()
                }
            }
        }
    }
}
