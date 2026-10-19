pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme
import EchoMusic.Shell

// Used only when settings recovery fails before any SQL/API/audio owner is safe to create.
ApplicationWindow {
    id: startupWindow
    required property StartupController startup
    required property WindowController windowService
    required property ShellSettings shellSettings
    width: 1150
    height: 750
    minimumWidth: 700
    minimumHeight: 550
    visible: true
    title: "EchoMusicNext"
    flags: Qt.platform.os === "osx" ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint : Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    font: Theme.bodyFont

    StartupPage {
        width: startupWindow.width / startupWindow.shellSettings.uiScale
        height: startupWindow.height / startupWindow.shellSettings.uiScale
        scale: startupWindow.shellSettings.uiScale
        transformOrigin: Item.TopLeft
        startup: startupWindow.startup
        settings: startupWindow.shellSettings
        windowService: startupWindow.windowService
        window: startupWindow
    }
}
