pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme
import EchoMusic.Controls
import EchoMusic.Shell
import EchoMusic.Settings

SettingsSection {
    id: section
    required property ShellSettings settings
    required property WindowController windowService
    property LogService logging: null
    property RelaunchService relaunch: null
    heading: qsTr("实验性功能")
    glyph: "flask"
    SettingsRow {
        heading: qsTr("页面缓存")
        description: qsTr("保留已访问的页面，返回时无需重新加载")
        EchoSwitch {
            objectName: "settingsKeepAlive"
            checked: section.settings.keepAliveEnabled
            Accessible.name: qsTr("页面缓存")
            onToggled: section.settings.keepAliveEnabled = checked
        }
    }
    SettingsDivider {
        visible: section.settings.keepAliveEnabled
    }
    SettingsRow {
        visible: section.settings.keepAliveEnabled
        heading: qsTr("最大缓存页面数")
        description: qsTr("超出后自动释放最早缓存的页面，避免占用过多内存")
        EchoSpinBox {
            objectName: "settingsCacheMaximum"
            implicitWidth: 180
            from: 3
            to: 30
            value: section.settings.keepAliveMax
            Accessible.name: qsTr("最大缓存页面数")
            onValueModified: section.settings.keepAliveMax = value
        }
    }
    SettingsDivider {
        visible: section.logging !== null
    }
    SettingsRow {
        visible: section.logging !== null
        heading: qsTr("日志级别")
        description: qsTr("标准模式只记录关键运行信息、警告和错误")
        EchoSelect {
            objectName: "settingsLogLevel"
            implicitWidth: 180
            options: [
                {
                    label: qsTr("标准"),
                    value: "info"
                },
                {
                    label: qsTr("详细"),
                    value: "debug"
                },
                {
                    label: qsTr("极详细"),
                    value: "verbose"
                },
                {
                    label: qsTr("仅警告"),
                    value: "warn"
                },
                {
                    label: qsTr("仅错误"),
                    value: "error"
                }
            ]
            value: section.logging ? section.logging.level : "info"
            colors: Theme.floating
            Accessible.name: qsTr("日志级别")
            onValueEdited: value => {
                if (section.logging)
                    section.logging.level = value;
            }
        }
    }
    SettingsDivider {
        visible: section.logging !== null
    }
    SettingsRow {
        visible: section.logging !== null
        heading: qsTr("API 响应体日志")
        description: qsTr("仅排查接口问题时开启，内容会裁剪并脱敏")
        EchoSwitch {
            objectName: "settingsLogApiResponse"
            checked: section.logging ? section.logging.apiResponseBody : false
            Accessible.name: qsTr("API 响应体日志")
            onToggled: if (section.logging)
                section.logging.apiResponseBody = checked
        }
    }
    SettingsDivider {
        visible: section.logging !== null
    }
    SettingsRow {
        visible: section.logging !== null
        heading: qsTr("临时诊断日志")
        controlGap: 8
        description: section.logging && section.logging.diagnosticActive ? qsTr("剩余约 %1 分钟").arg(section.logging.diagnosticMinutes) : qsTr("未启用")
        EchoSoftButton {
            objectName: "settingsDisableDiagnostics"
            visible: section.logging && section.logging.diagnosticActive
            text: qsTr("关闭")
            colors: Theme.floating
            onClicked: if (section.logging)
                section.logging.disableTemporaryDiagnostics()
        }
        EchoSoftButton {
            objectName: "settingsEnableDiagnostics"
            text: qsTr("10 分钟")
            colors: Theme.floating
            onClicked: if (section.logging)
                section.logging.enableTemporaryDiagnostics()
        }
        EchoSoftButton {
            objectName: "settingsRestartDiagnostics"
            text: section.relaunch && section.relaunch.automaticRestart ? qsTr("诊断并重启") : qsTr("开启启动诊断")
            enabled: section.relaunch !== null && !section.relaunch.busy
            colors: Theme.floating
            onClicked: {
                if (section.logging && section.logging.enableTemporaryDiagnostics())
                    section.relaunch.restart();
            }
        }
    }
}
