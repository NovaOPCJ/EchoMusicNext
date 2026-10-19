pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Settings

SettingsTextDialog {
    id: dialog
    required property AboutService about
    readonly property bool available: about.resultState === AboutService.UpdateAvailable
    readonly property bool downloading: about.downloadState === AboutService.Downloading
    readonly property bool downloaded: about.downloadState === AboutService.Downloaded
    readonly property bool installing: about.downloadState === AboutService.Installing
    markdown: true
    preferredWidth: 520
    heading: available ? qsTr("发现新版本 v%1").arg(about.releaseVersion) : about.resultState === AboutService.AlreadyLatest ? qsTr("已是最新版本") : about.resultState === AboutService.NoPublishedRelease ? qsTr("暂无可用更新") : qsTr("检查更新失败")
    description: available ? qsTr("当前版本 v%1").arg(about.version) : about.statusText
    text: available ? about.releaseNotes.length ? about.releaseNotes : qsTr("暂无更新日志。") : ""
    onClosed: about.closeResult()
    footerContent: ColumnLayout {
        spacing: 12
        Text {
            Layout.fillWidth: true
            visible: dialog.available && dialog.about.updateMessage.length > 0
            text: dialog.about.updateMessage
            wrapMode: Text.WordWrap
            color: Theme.floatingSecondaryText
            font: Theme.font(12, Theme.regularWeight)
        }
        Text {
            Layout.fillWidth: true
            visible: dialog.about.downloadError.length > 0
            text: dialog.about.downloadError
            wrapMode: Text.WordWrap
            color: Theme.floatingSecondaryText
            font: Theme.font(12, Theme.regularWeight)
        }
        ProgressBar {
            id: progress
            Layout.fillWidth: true
            visible: dialog.downloading
            from: 0
            to: 100
            value: dialog.about.downloadPercent
            Accessible.name: qsTr("更新下载进度")
            background: Rectangle {
                implicitHeight: 4
                radius: 2
                color: Theme.withAlpha(Theme.floatingText, 0.18)
            }
            contentItem: Item {
                implicitHeight: 4
                Rectangle {
                    width: progress.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color: Theme.floating.accent
                }
            }
        }
        Text {
            visible: dialog.downloading || dialog.downloaded || dialog.installing
            text: dialog.downloading ? qsTr("正在下载 %1%（关闭后继续下载）").arg(Math.floor(dialog.about.downloadPercent)) : dialog.installing ? qsTr("正在准备安装，随后将退出应用…") : qsTr("更新包已下载并通过校验")
            color: Theme.floatingSecondaryText
            font: Theme.font(12, Theme.regularWeight)
        }
        RowLayout {
            Layout.fillWidth: true
            EchoSoftButton {
                visible: dialog.available
                text: qsTr("发布说明")
                colors: Theme.floating
                onClicked: dialog.about.openRelease()
            }
            Item {
                Layout.fillWidth: true
            }
            EchoSoftButton {
                text: dialog.available ? qsTr("稍后") : qsTr("关闭")
                colors: Theme.floating
                onClicked: dialog.close()
            }
            EchoSoftButton {
                visible: dialog.downloading
                text: qsTr("取消下载")
                colors: Theme.floating
                onClicked: dialog.about.cancelDownload()
            }
            EchoSoftButton {
                visible: dialog.available && !dialog.downloading
                enabled: !dialog.installing
                text: dialog.downloaded ? qsTr("安装并重启") : dialog.installing ? qsTr("准备安装…") : dialog.about.downloadState === AboutService.DownloadFailed ? qsTr("重新下载") : dialog.about.downloadLabel
                accent: true
                colors: Theme.floating
                onClicked: dialog.downloaded ? dialog.about.install() : dialog.about.download()
            }
        }
    }
}
