pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Shell
import EchoMusic.Playback

SettingsSection {
    id: section
    required property ShellSettings settings
    required property WindowController windowService
    required property PlaybackService playback
    property T.Popup surface: null
    readonly property CastService output: playback.cast
    property string pinTarget: ""
    readonly property bool resourceActive: visible && Window.window !== null && Window.window.visible && Window.window.visibility !== Window.Minimized
    heading: qsTr("投放")
    glyph: "cast"
    onResourceActiveChanged: output.setBrowsingFor(section, resourceActive)
    Component.onCompleted: output.setBrowsingFor(section, resourceActive)
    Component.onDestruction: output.setBrowsingFor(section, false)
    Connections {
        target: section.output
        function onChanged() {
            if (!section.output.enabled || !section.output.busy && section.output.remote && !section.output.errorMessage.length)
                section.pinTarget = "";
        }
    }
    SettingsRow {
        heading: qsTr("启用投放")
        description: qsTr("搜索并连接 DLNA / AirPlay 设备")
        EchoSwitch {
            objectName: "settingsCastEnabled"
            checked: section.output.enabled
            colors: Theme.floating
            Accessible.name: qsTr("启用投放")
            onToggled: section.output.enabled = checked
        }
    }
    SettingsDivider {}
    SettingsRow {
        heading: qsTr("当前输出")
        description: section.output.activeTitle
        statusText: section.output.errorMessage.length ? section.output.errorMessage : section.output.searching && section.output.deviceCount === 0 ? qsTr("正在搜索投放设备...") : ""
        statusColor: section.output.errorMessage.length ? Theme.floating.dangerText : Theme.floatingSecondaryText
        controlGap: 8
        EchoSoftButton {
            visible: section.output.remote
            text: section.output.busy ? qsTr("停止中") : qsTr("停止投放")
            enabled: !section.output.busy
            colors: Theme.floating
            onClicked: section.output.useLocal()
        }
        EchoSoftButton {
            text: section.output.searching ? qsTr("搜索中") : qsTr("刷新")
            enabled: section.output.enabled && !section.output.busy && !section.output.searching
            colors: Theme.floating
            onClicked: section.output.refresh()
        }
    }
    SettingsDivider {}
    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: false
        EchoTextTagRow {
            text: qsTr("可用设备")
            font: Theme.font(13, Theme.boldWeight)
            color: Theme.floatingText
        }
        Item {
            Layout.fillWidth: true
        }
        Label {
            text: section.output.enabled ? qsTr("%1 台").arg(section.output.deviceCount) : qsTr("未启用")
            font: Theme.font(12, Theme.boldWeight)
            color: Theme.floatingSecondaryText
        }
    }
    GridLayout {
        Layout.fillWidth: true
        columns: section.Window.window && section.Window.window.width / section.settings.uiScale <= 760 ? 1 : 2
        columnSpacing: 14
        rowSpacing: 14
        CastDeviceGroup {
            popupSurface: section.surface
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignTop
            output: section.output
            heading: "DLNA"
            deviceModel: section.output.dlna
            maximumListHeight: Math.max(72, Math.min(320, section.Window.height / section.settings.uiScale - 360))
            onChoose: (id, needsPin) => section.output.connectTarget(id)
        }
        CastDeviceGroup {
            popupSurface: section.surface
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignTop
            output: section.output
            heading: "AirPlay"
            deviceModel: section.output.airplay
            maximumListHeight: Math.max(72, Math.min(320, section.Window.height / section.settings.uiScale - 360))
            pinTarget: section.pinTarget
            onChoose: (id, needsPin) => {
                if (needsPin)
                    section.pinTarget = id;
                else
                    section.output.connectTarget(id);
            }
            onSubmitPin: value => section.output.connectTarget(section.pinTarget, value)
        }
    }
    SettingsDivider {}
    SettingsRow {
        heading: qsTr("清除设备记录")
        description: qsTr("清除各设备记住的音量和本机保存的 AirPlay 配对。")
        EchoSoftButton {
            text: qsTr("清除")
            danger: true
            enabled: !section.output.busy
            colors: Theme.floating
            onClicked: section.output.clearRecords()
        }
    }
}
