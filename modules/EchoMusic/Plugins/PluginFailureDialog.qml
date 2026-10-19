pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme
import EchoMusic.Controls
import EchoMusic.Plugins

EchoDialog {
    id: dialog
    objectName: "pluginFailureDialog"
    required property PluginRuntime runtime
    required property string installationId
    property int recordRevision: 0
    // Read projection invalidated by the native catalog, including callback failures.
    readonly property var failureRecord: recordRevision >= 0 ? runtime.record(installationId) : ({})
    readonly property var failure: failureRecord.failure || ({})
    Connections {
        target: dialog.runtime
        function onCatalogChanged() {
            ++dialog.recordRevision;
        }
    }
    heading: failureRecord.name && failure.reason ? failureRecord.name + " · " + failure.reason : qsTr("插件异常详情")
    preferredWidth: 560
    flushBody: true
    body: Component {
        Flickable {
            id: viewport
            implicitHeight: detail.implicitHeight
            contentWidth: width
            contentHeight: detail.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: EchoPopupScrollBar {
                surface: dialog
                view: viewport
                resourceActive: dialog.visible
            }
            ColumnLayout {
                id: detail
                width: viewport.width - 24
                spacing: 14
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: detailsGrid.implicitHeight + 28
                    radius: Theme.controlRadius
                    color: Theme.floating.mutedSurface
                    border.color: Theme.floating.subtleBorder
                    GridLayout {
                        id: detailsGrid
                        x: 14
                        y: 14
                        width: parent.width - 28
                        columns: 2
                        columnSpacing: 14
                        rowSpacing: 8
                        Repeater {
                            model: [qsTr("类型"), dialog.failure.reason || qsTr("运行异常"), qsTr("位置"), dialog.failure.source || (dialog.failureRecord.error ? qsTr("当前运行状态") : qsTr("最近一次插件异常")), qsTr("时间"), dialog.failure.time ? new Date(dialog.failure.time).toLocaleString(Qt.locale(), "yyyy/M/d HH:mm:ss") : "—"]
                            delegate: Label {
                                required property int index
                                required property string modelData
                                Layout.preferredWidth: index % 2 ? -1 : 64
                                Layout.fillWidth: index % 2 === 1
                                Layout.minimumWidth: 0
                                text: modelData
                                font: Theme.font(12, Theme.boldWeight)
                                color: index % 2 ? Theme.floatingText : Theme.floatingSecondaryText
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: message.implicitHeight + 28
                    radius: Theme.controlRadius
                    color: Theme.floating.mutedSurface
                    border.color: Theme.floating.subtleBorder
                    ColumnLayout {
                        id: message
                        x: 14
                        y: 14
                        width: parent.width - 28
                        spacing: 8
                        Label {
                            text: qsTr("错误信息")
                            font: Theme.font(12, Theme.extraBoldWeight)
                            color: Theme.floatingText
                        }
                        Label {
                            objectName: "pluginFailureText"
                            Layout.fillWidth: true
                            text: dialog.failure.message || dialog.failureRecord.error || ""
                            font: Theme.font(13, Theme.regularWeight)
                            color: Theme.floatingSecondaryText
                            wrapMode: Text.WrapAnywhere
                            lineHeightMode: Text.FixedHeight
                            lineHeight: 20.8
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: stackContent.implicitHeight + 28
                    visible: !!dialog.failure.stack
                    radius: Theme.controlRadius
                    color: Theme.floating.mutedSurface
                    border.color: Theme.floating.subtleBorder
                    ColumnLayout {
                        id: stackContent
                        x: 14
                        y: 14
                        width: parent.width - 28
                        spacing: 8
                        Label {
                            text: qsTr("调用栈")
                            font: Theme.font(12, Theme.extraBoldWeight)
                            color: Theme.floatingText
                        }
                        EchoTextArea {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.min(220, implicitHeight)
                            readOnly: true
                            text: dialog.failure.stack || ""
                            font: Theme.font(11, Theme.regularWeight)
                            colors: Theme.floating
                            wrapMode: TextEdit.WrapAnywhere
                        }
                    }
                }
            }
        }
    }
    footerContent: Component {
        RowLayout {
            spacing: 12
            PluginActionButton {
                danger: true
                colors: Theme.floating
                objectName: "pluginClearFailure"
                text: qsTr("清除记录")
                visible: !dialog.failureRecord.error
                onClicked: {
                    dialog.runtime.clearFailure(dialog.installationId);
                    dialog.close();
                }
            }
            Item {
                Layout.fillWidth: true
            }
            PluginActionButton {
                colors: Theme.floating
                text: qsTr("关闭")
                onClicked: dialog.close()
            }
        }
    }
}
