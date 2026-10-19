pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme
import EchoMusic.Controls
import EchoMusic.Shell

// A fixed-width window-action group shared by main and overlay headers.
WindowActionGroup {
    id: actions
    required property WindowController service
    required property ShellSettings settings
    property ThemeColors colors: Theme.colors
    property bool nativeCaption: service.macOS
    property bool compact: false
    property bool showMiniPlayer: true
    EchoIconButton {
        hoverWhenDisabled: true
        objectName: "windowMiniPlayerAction"
        visible: actions.showMiniPlayer && (!actions.compact || actions.service.fullscreen)
        glyph: "picture-in-picture"
        text: qsTr("Mini 播放器")
        colors: actions.colors
        padding: 9
        onClicked: actions.service.floating.toggleMini()
    }
    EchoIconButton {
        hoverWhenDisabled: true
        objectName: "windowFullscreenAction"
        visible: !actions.nativeCaption && actions.settings.showFullscreenButton && (!actions.compact || actions.service.fullscreen)
        glyph: "arrows-maximize"
        text: actions.service.fullscreen ? qsTr("退出全屏") : qsTr("全屏")
        Accessible.name: qsTr("切换全屏")
        colors: actions.colors
        padding: 10
        onClicked: actions.service.toggleFullscreen()
    }
    WindowCaptionControls {
        objectName: "windowCaptionControls"
        visible: !actions.nativeCaption && !actions.service.fullscreen
        service: actions.service
        colors: actions.colors
    }
}
