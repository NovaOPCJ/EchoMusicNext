pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Shell

// ShellSettings owns consent. HomePage creates and disposes the complete
// agreement surface on demand. Wording must match the project license and cache behavior.
EchoDialog {
    id: dialog
    objectName: "userAgreementDialog"
    required property ShellSettings settings
    signal declineRequested
    heading: qsTr("用户条款")
    preferredWidth: 600
    maximumHeight: 560
    windowHeightMargin: 140
    flushBody: true
    closable: false
    closePolicy: Popup.NoAutoClose
    readonly property var sections: [
        {
            title: qsTr("项目性质"),
            content: qsTr("本程序是酷狗音乐第三方客户端，非官方产品。如需完整功能体验，请使用官方客户端。")
        },
        {
            title: qsTr("使用范围"),
            content: qsTr("本项目面向个人学习和技术研究。软件的使用、修改与再分发遵循 GPL-3.0-only 协议，请勿用于违法行为。")
        },
        {
            title: qsTr("版权内容"),
            content: qsTr("本项目通过公开接口获取音乐数据，所有音乐内容版权归原平台及版权方所有。使用过程中产生的缓存数据应在 24 小时内清除。")
        },
        {
            title: qsTr("免责条款"),
            content: qsTr("开发者不对因使用本项目导致的任何直接或间接损失承担责任，包括但不限于数据丢失、设备故障、法律纠纷等。")
        },
        {
            title: qsTr("法律责任"),
            content: qsTr("您应遵守所在地区的法律法规使用本项目。因违法使用导致的一切法律后果由使用者自行承担。")
        },
        {
            title: qsTr("版权尊重"),
            content: qsTr("请尊重音乐创作者和平台的劳动成果，支持正版音乐。本项目不接受任何商业合作、广告或捐赠。")
        },
        {
            title: qsTr("争议处理"),
            content: qsTr("如版权方对本项目有异议，请通过 GitHub Issues 联系开发者，我们将及时处理。")
        }
    ]
    body: Flickable {
        id: scroll
        implicitHeight: column.implicitHeight
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: column
            width: scroll.width - 24
            spacing: 12
            Repeater {
                model: dialog.sections
                EchoLegalSection {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    heading: modelData.title
                    text: modelData.content
                }
            }
            EchoTextTagRow {
                Layout.fillWidth: true
                Layout.topMargin: 8
                fillTextWidth: true
                wrap: true
                text: qsTr("点击“同意并继续”即表示您已阅读并接受以上全部条款。")
                font: Theme.font(13, Theme.semiboldWeight)
                lineHeight: 24
                color: Theme.floatingText
            }
        }
        ScrollBar.vertical: EchoPopupScrollBar {
            surface: dialog
            view: scroll
        }
    }
    footerContent: RowLayout {
        spacing: 8
        Item {
            Layout.fillWidth: true
        }
        EchoSoftButton {
            objectName: "userAgreementReject"
            Layout.minimumWidth: 108
            colors: Theme.floating
            danger: true
            text: qsTr("不同意并退出")
            font: Theme.font(13, Theme.semiboldWeight)
            onClicked: dialog.declineRequested()
        }
        EchoButton {
            objectName: "userAgreementAccept"
            Layout.minimumWidth: 108
            colors: Theme.floating
            text: qsTr("同意并继续")
            font: Theme.font(13, Theme.semiboldWeight)
            onClicked: {
                dialog.settings.acceptUserAgreement();
                dialog.close();
            }
        }
    }
}
