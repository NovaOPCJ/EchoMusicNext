pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Settings

SettingsTextDialog {
    id: dialog
    required property AboutService about
    markdown: true
    preferredWidth: 520
    heading: about.resultState === AboutService.UpdateAvailable ? qsTr("发现新版本 v%1").arg(about.releaseVersion) : about.resultState === AboutService.AlreadyLatest ? qsTr("已是最新版本") : about.resultState === AboutService.NoPublishedRelease ? qsTr("暂无可用更新") : qsTr("检查更新失败")
    description: about.resultState === AboutService.UpdateAvailable ? qsTr("当前版本 v%1").arg(about.version) : about.resultState === AboutService.AlreadyLatest ? qsTr("当前版本 v%1 已是最新版本。").arg(about.version) : about.statusText
    text: about.resultState === AboutService.UpdateAvailable ? about.releaseNotes.length ? about.releaseNotes : qsTr("暂未获取到更新日志，可前往发布页查看。") : ""
    onClosed: about.closeResult()
    footerContent: RowLayout {
        Item {
            Layout.fillWidth: true
        }
        EchoSoftButton {
            text: dialog.about.resultState === AboutService.UpdateAvailable ? qsTr("稍后") : qsTr("关闭")
            colors: Theme.floating
            onClicked: dialog.close()
        }
        EchoSoftButton {
            visible: dialog.about.resultState === AboutService.UpdateAvailable
            text: qsTr("前往发布页下载")
            accent: true
            colors: Theme.floating
            onClicked: dialog.about.openRelease()
        }
    }
}
