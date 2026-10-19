pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Account

Item {
    id: panel
    objectName: "loginQrPanel"
    required property QrLoginService service
    property int provider: 0
    readonly property bool creating: service.state === QrLoginService.Creating || service.state === QrLoginService.Idle
    readonly property bool scanned: service.state === QrLoginService.Scanned
    readonly property bool authorizing: service.state === QrLoginService.Authorizing
    readonly property bool expired: service.state === QrLoginService.Expired
    readonly property bool failed: service.state === QrLoginService.Error
    readonly property color providerColor: provider === 3 ? "#12b7f5" : provider === 4 ? "#07c160" : Theme.accent
    readonly property color stateForeground: provider === 0 ? Theme.accentText : Theme.dark ? providerColor : Qt.darker(providerColor, 1.4)
    implicitHeight: heading.height + 10 + qrSurface.height + 24 + statusRow.height
    height: implicitHeight

    // The subtitle has equal gaps to the title and code. Keep intrinsic flow
    // rather than distributing surplus height through a stretched layout.
    Column {
        id: heading
        width: parent.width
        spacing: 10
        Label {
            objectName: "loginQrTitle"
            width: parent.width
            height: 32.5
            text: panel.provider === 3 ? qsTr("QQ 登录") : panel.provider === 4 ? qsTr("微信登录") : qsTr("扫码登录")
            color: Theme.text
            font: Theme.spacedFont(26, Theme.blackWeight, panel.provider === 0 ? -0.65 : 0)
            horizontalAlignment: Text.AlignHCenter
        }
        Label {
            objectName: "loginQrSubtitle"
            width: parent.width
            height: 19.5
            text: panel.provider === 3 ? qsTr("请使用 QQ 扫描二维码") : panel.provider === 4 ? qsTr("请使用微信扫描二维码") : qsTr("使用酷狗概念版扫码")
            color: Theme.secondaryText
            font: Theme.spacedFont(13, Theme.boldWeight, 1.5)
            horizontalAlignment: Text.AlignHCenter
        }
    }
    // This is a transparent layout region, not a card. The image owns the
    // scan quiet zone; the waiting code has no rounded corners or cropped pixels.
    Item {
        id: qrSurface
        objectName: "loginQrSurface"
        anchors.top: heading.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        width: 204
        height: 204
        Item {
            objectName: "loginQrImageViewport"
            anchors.centerIn: parent
            // The status veil would tint the white quiet zone into a frame on
            // dark surfaces. Hide only that margin while the code is covered;
            // the waiting/scannable image retains all of its original pixels.
            width: Theme.dark && panel.service.state !== QrLoginService.Waiting ? 162 : 204
            height: width
            clip: true
            Image {
                objectName: "loginQrImage"
                anchors.centerIn: parent
                width: 204
                height: 204
                visible: panel.service.imageDataUrl.length > 0
                source: panel.visible && visible ? panel.service.imageDataUrl : ""
                sourceSize.width: Math.ceil(width * Screen.devicePixelRatio)
                sourceSize.height: Math.ceil(height * Screen.devicePixelRatio)
                fillMode: Image.PreserveAspectFit
                smooth: false
                asynchronous: true
                cache: false
            }
        }
        Rectangle {
            objectName: "loginQrStateOverlay"
            anchors.fill: parent
            z: 1
            visible: panel.service.state !== QrLoginService.Waiting
            // A state is a layer over the same image/quiet zone, not another
            // layout. Keep the QR texture underneath; no border/radius card.
            color: Theme.withAlpha(Theme.elevated, panel.creating ? 1 : 0.94)
            Rectangle {
                objectName: "loginQrStateIcon"
                anchors.centerIn: parent
                width: 48
                height: 48
                radius: 24
                color: Theme.withAlpha(panel.failed ? Theme.colors.danger : panel.expired ? Theme.secondaryText : panel.providerColor, 0.1)
                EchoIcon {
                    anchors.centerIn: parent
                    width: 26
                    height: 26
                    name: panel.failed ? "info-circle" : panel.expired ? "clock" : "check"
                    color: panel.failed ? Theme.colors.dangerText : panel.expired ? Theme.secondaryText : panel.stateForeground
                    visible: !panel.creating && !panel.authorizing
                }
                EchoSpinner {
                    anchors.centerIn: parent
                    width: 26
                    height: 26
                    color: panel.stateForeground
                    visible: panel.creating || panel.authorizing
                    resourceActive: panel.visible
                }
            }
        }
    }
    Item {
        id: statusRow
        objectName: "loginQrStatusRow"
        anchors.top: qrSurface.bottom
        anchors.topMargin: 24
        width: parent.width
        height: Math.max(16.5, status.implicitHeight)
        Label {
            id: status
            objectName: "loginQrStatus"
            anchors.centerIn: parent
            width: parent.width - 64
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            text: panel.creating ? qsTr("正在生成二维码") : panel.authorizing ? qsTr("正在登录") : panel.scanned ? (panel.provider === 3 ? qsTr("已扫码，请在 QQ 中确认登录") : panel.provider === 4 ? qsTr("已扫码，请在微信中确认登录") : qsTr("已扫码，请在酷狗概念版中确认登录")) : panel.expired ? qsTr("二维码已过期") : panel.failed ? (panel.service.errorMessage || qsTr("二维码加载失败")) : panel.provider === 3 ? qsTr("等待 QQ 扫码") : panel.provider === 4 ? qsTr("等待微信扫码") : qsTr("等待酷狗扫码")
            color: Theme.secondaryText
            font: Theme.spacedFont(11, Theme.blackWeight, panel.failed || panel.scanned ? 0 : 3)
        }
        EchoIconButton {
            id: refreshButton
            objectName: "loginQrRefresh"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            padding: 7
            visible: !panel.creating && !panel.authorizing
            glyph: "refresh"
            text: qsTr("重新生成二维码")
            foreground: hovered ? panel.providerColor : Theme.colors.iconMain
            background: Rectangle {
                radius: Theme.controlRadius
                color: refreshButton.hovered ? Theme.withAlpha(panel.providerColor, 0.1) : "transparent"
            }
            onClicked: panel.service.start()
        }
    }
}
