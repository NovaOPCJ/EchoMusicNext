pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme
import EchoMusic.Controls
import EchoMusic.Shell
import EchoMusic.Plugins

Item {
    id: page
    objectName: "pluginShareResolvePage"
    required property PluginManager manager
    required property RouteController router
    required property RouteContext context
    required property ShellSettings settings
    readonly property string resolveState: manager.shareState
    readonly property bool working: manager.busy || manager.catalogLoading || manager.runtime.loading
    function management() {
        manager.search = manager.shareRecord.id || "";
        router.navigate("plugin-management", {
            view: "marketplace"
        });
    }
    Item {
        anchors.centerIn: parent
        width: Math.min(680, parent.width - 48)
        height: content.implicitHeight + 56
        EchoFloatingSurface {
            anchors.fill: parent
            radius: Theme.controlRadius
            color: Theme.card
            border.color: Theme.border
        }
        ColumnLayout {
            id: content
            x: 28
            y: 28
            width: parent.width - 56
            spacing: 20
            EchoTag {
                text: qsTr("插件分享")
                tone: EchoTag.Accent
                implicitHeight: 30
                contentComponent: Component {
                    Label {
                        text: qsTr("插件分享")
                        font: Theme.font(12, Theme.extraBoldWeight)
                        color: Theme.accentText
                    }
                }
            }
            RowLayout {
                Layout.fillHeight: false
                Layout.fillWidth: true
                spacing: 16
                Rectangle {
                    implicitWidth: 64
                    implicitHeight: 64
                    Layout.alignment: Qt.AlignTop
                    radius: Theme.controlRadius
                    color: Theme.withAlpha(Theme.accent, 0.12)
                    border.color: Theme.withAlpha(Theme.accent, 0.22)
                    EchoSpinner {
                        anchors.centerIn: parent
                        visible: page.working || page.resolveState === "loading"
                        width: 32
                        height: 32
                    }
                    EchoIcon {
                        anchors.centerIn: parent
                        visible: !page.working && page.resolveState !== "loading"
                        width: 32
                        height: 32
                        name: page.resolveState === "failed" ? "alert-triangle" : page.resolveState === "installed" ? "check" : "puzzle"
                        color: Theme.accentText
                    }
                }
                ColumnLayout {
                    Layout.fillHeight: false
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        Layout.fillWidth: true
                        text: page.working ? qsTr("正在处理分享的插件") : page.resolveState === "need-source" ? qsTr("需要添加插件源") : page.resolveState === "ready" ? page.manager.shareRecord.updateAvailable ? qsTr("更新分享的插件") : qsTr("安装分享的插件") : page.resolveState === "installed" ? qsTr("插件已安装") : page.resolveState === "failed" ? qsTr("分享的插件打不开") : qsTr("正在打开分享的插件")
                        font: Theme.font(26, Theme.extraBoldWeight)
                        color: Theme.text
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: page.resolveState === "failed" ? page.manager.shareError : page.resolveState === "need-source" ? qsTr("分享的插件来自尚未添加或已停用的插件源。确认添加或启用后，将继续查找插件。") : page.resolveState === "installed" ? qsTr("当前安装版本已包含分享的版本，可前往插件管理页。") : page.resolveState === "ready" ? qsTr("请确认来源后安装。插件是可信扩展，可以访问声明的宿主能力。") : qsTr("正在读取插件源和插件清单。")
                        font: Theme.font(14, Theme.regularWeight)
                        color: Theme.secondaryText
                        wrapMode: Text.WordWrap
                        lineHeight: 1.7
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: details.implicitHeight + 28
                radius: Theme.controlRadius
                color: Theme.mutedSurface
                border.color: Theme.border
                ColumnLayout {
                    id: details
                    x: 14
                    y: 14
                    width: parent.width - 28
                    spacing: 8
                    Label {
                        Layout.fillWidth: true
                        visible: !!page.manager.shareRecord.name
                        text: page.manager.shareRecord.name || ""
                        font: Theme.font(15, Theme.extraBoldWeight)
                        color: Theme.text
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        text: page.manager.shareSourceUrl
                        font: Theme.font(12, Theme.mediumWeight)
                        color: Theme.secondaryText
                        wrapMode: Text.WrapAnywhere
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !!page.manager.shareRecord.version
                        text: qsTr("插件源版本 %1 · 分享版本 %2").arg(page.manager.shareRecord.version || "").arg(page.manager.shareRecord.sharedVersion || "")
                        font: Theme.font(12, Theme.mediumWeight)
                        color: Theme.secondaryText
                    }
                }
            }
            Flow {

                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                spacing: 10
                EchoButton {
                    visible: page.resolveState === "need-source"
                    text: qsTr("添加或启用插件源")
                    size: EchoControlSize.Standard
                    enabled: !page.working
                    onClicked: page.manager.acceptShareSource()
                }
                EchoButton {
                    visible: page.resolveState === "ready"
                    text: page.manager.shareRecord.updateAvailable ? qsTr("更新插件") : qsTr("安装插件")
                    size: EchoControlSize.Standard
                    enabled: !page.working
                    onClicked: page.manager.installShared()
                }
                EchoButton {
                    visible: page.resolveState === "failed"
                    text: qsTr("重新打开")
                    glyph: "refresh"
                    size: EchoControlSize.Standard
                    enabled: !page.working
                    onClicked: page.manager.refreshCatalog(true)
                }
                EchoButton {
                    accent: false
                    text: qsTr("插件管理")
                    glyph: "arrow-left"
                    size: EchoControlSize.Standard
                    enabled: !page.working
                    onClicked: page.management()
                }
            }
        }
    }
}
