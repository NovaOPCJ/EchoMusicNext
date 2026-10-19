pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Account

Item {
    id: profile
    required property AccountController service
    required property real uiScale
    property bool collapsed: false
    signal profileRequested
    implicitHeight: collapsed ? 42 : 56
    Rectangle {
        visible: !profile.collapsed
        anchors.fill: parent
        radius: Theme.cardRadius
        color: userLink.hovered || accountTrigger.hovered ? Theme.hover : Theme.mutedSurface
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: profile.collapsed ? (profile.width - 42) / 2 : 8
        anchors.rightMargin: profile.collapsed ? (profile.width - 42) / 2 : 4
        anchors.topMargin: profile.collapsed ? 0 : 4
        anchors.bottomMargin: profile.collapsed ? 0 : 4
        spacing: profile.collapsed ? 0 : 3
        Button {
            id: userLink
            objectName: "sidebarUserLink"
            Layout.fillWidth: !profile.collapsed

            Layout.preferredWidth: profile.collapsed ? 42 : -1
            Layout.preferredHeight: profile.collapsed ? 42 : 44
            padding: 0
            leftPadding: 0
            rightPadding: 0
            topPadding: 0
            bottomPadding: 0
            hoverEnabled: true
            Accessible.name: profile.service.account.userId ? profile.service.account.nickname : qsTr("点击登录账号")
            onClicked: profile.service.account.userId ? profile.profileRequested() : profile.service.openLogin()
            background: Rectangle {
                objectName: "sidebarUserLinkBackground"
                radius: Theme.cardRadius
                color: profile.collapsed ? userLink.hovered ? Theme.hover : Theme.mutedSurface : "transparent"
            }
            contentItem: RowLayout {
                spacing: 10
                AccountAvatar {
                    objectName: "sidebarAccountAvatar"
                    Layout.alignment: Qt.AlignVCenter
                    Layout.fillWidth: profile.collapsed
                    Layout.leftMargin: profile.collapsed ? 5 : 0
                    Layout.rightMargin: profile.collapsed ? 5 : 0
                    Layout.preferredWidth: profile.collapsed ? 32 : 36
                    Layout.maximumWidth: profile.collapsed ? 32 : 36
                    Layout.preferredHeight: profile.collapsed ? 32 : 36
                    source: profile.service.account.avatarUrl
                    identityIcon: profile.service.account.avatarIdentityIcon
                    identityLabel: profile.service.account.avatarIdentityLabel
                    placeholderOpacity: profile.collapsed ? 0.3 : 0.1
                }
                ColumnLayout {
                    Layout.fillHeight: false
                    visible: !profile.collapsed
                    Layout.fillWidth: true
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: profile.service.account.userId ? profile.service.account.nickname : qsTr("未登录")
                        elide: Text.ElideRight
                        color: Theme.text
                        font: Theme.font(14, Theme.semiboldWeight)
                        lineHeightMode: Text.FixedHeight
                        lineHeight: 20
                    }
                    EchoTextTagRow {
                        objectName: "sidebarAccountMeta"
                        Layout.fillWidth: true
                        spacing: 6
                        lineHeight: 16
                        text: profile.service.account.userId ? "Lv." + profile.service.account.grade : qsTr("点击登录账号")
                        color: Theme.secondaryText
                        font: Theme.font(11, Theme.regularWeight)
                        MembershipTag {
                            objectName: "sidebarMembershipTag"
                            musicTone: "#07c160"
                            visible: profile.service.account.userId > 0
                            kind: profile.service.account.memberships.length ? profile.service.account.memberships[0].kind : "none"
                            text: profile.service.account.membershipLabel || "NOVIP"
                        }
                    }
                }
            }
        }
        Rectangle {
            visible: !profile.collapsed
            Layout.preferredWidth: 1
            Layout.preferredHeight: 18
            color: Theme.withAlpha(Theme.store.tone, 0.1)
        }
        EchoIconButton {
            id: accountTrigger
            visible: !profile.collapsed
            objectName: "sidebarAccountTrigger"
            Layout.preferredWidth: profile.collapsed ? 22 : 28
            Layout.preferredHeight: profile.collapsed ? 42 : 44
            padding: profile.collapsed ? 4 : 5
            background: Rectangle {
                color: "transparent"
                radius: Theme.controlRadius
                border.width: accountTrigger.visualFocus ? 2 : 0
                border.color: Theme.accent
            }
            contentItem: EchoIcon {
                name: accountTrigger.glyph
                color: accountTrigger.foreground
                rotation: panelOwner.visible ? 180 : 0
                Behavior on rotation {
                    NumberAnimation {
                        duration: Theme.fastDuration
                    }
                }
            }
            glyph: "chevron-down"
            text: qsTr("个人信息与账号切换")
            Accessible.role: Accessible.Button
            onClicked: panelOwner.visible ? panelOwner.close() : panelOwner.open()
        }
    }
    EchoToolTip {
        parent: userLink
        requestedVisible: profile.collapsed && userLink.hovered
        text: userLink.Accessible.name
    }
    EchoPopupLoader {
        id: panelOwner
        objectName: "sidebarAccountPopoverOwner"
        hostItem: profile
        sourceComponent: Component {
            AccountPopover {
                id: panel
                service: profile.service
                anchorItem: accountTrigger
                uiScale: profile.uiScale
                onSwitchRequested: switcherOwner.open()
                onLogoutRequested: logoutDialogOwner.open()
                onProfileRequested: profile.profileRequested()
            }
        }
    }
    EchoPopupLoader {
        id: switcherOwner
        objectName: "accountSwitcherDialogOwner"
        hostItem: profile
        sourceComponent: Component {
            AccountSwitcherDialog {
                id: switcher
                service: profile.service
                uiScale: profile.uiScale
            }
        }
    }
    EchoPopupLoader {
        id: logoutDialogOwner
        objectName: "logoutConfirmationOwner"
        hostItem: profile
        sourceComponent: Component {
            LogoutConfirmDialog {
                id: logoutDialog
                service: profile.service
                uiScale: profile.uiScale
            }
        }
    }
    Connections {
        target: profile.service.account
        function onChanged() {
            if (!profile.service.account.userId)
                panelOwner.close();
        }
    }
}
