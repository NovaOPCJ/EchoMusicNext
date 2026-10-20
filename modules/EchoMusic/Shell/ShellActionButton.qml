pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Shell

EchoIconButton {
    id: button
    required property ShellActionModel actions
    required property string actionKey
    required property string actionId
    required property string actionTitle
    required property string actionIcon
    required property bool actionEnabled
    required property bool actionActive
    required property string actionBadge
    required property bool badgeVisible
    property bool player: false
    property bool playbackLoading: false
    property bool lyricStyle: false
    property bool compact: false
    property int projection: ShellActionModel.All
    objectName: (player ? "playerbar-action-" : "titlebar-action-") + actionId
    glyph: actionIcon
    iconCovered: player && actionId === "play-toggle" && playbackLoading
    animatePlayPause: player && actionId === "play-toggle"
    playIconOffset: 1
    text: actionTitle
    tooltipText: actionId === "barrage" ? actionActive ? qsTr("弹幕已开启 · 设置与发送") : qsTr("弹幕已关闭 · 设置与发送") : text
    enabled: actionEnabled || player && actionId === "play-toggle" && playbackLoading
    hoverBackground: !player
    hoverWhenDisabled: !player
    implicitWidth: compact ? 28 : player && actionId === "play-toggle" ? 38 : player ? 36 : 34
    implicitHeight: compact ? 28 : player && actionId === "play-toggle" ? 38 : player ? 36 : 34
    padding: compact ? 4 : player && actionId === "play-toggle" ? 9 : player ? 8 : 7
    leftPadding: padding
    rightPadding: padding
    disabledOpacity: player ? playbackLoading && ["previous", "next"].includes(actionId) ? 0.75 : 0.6 : 0.4
    // Titlebar actions use neutral interaction colors.
    // The player accent is reserved for playback controls.
    foreground: player && actionId === "favorite" && actionActive ? button.colors.danger : player && (actionActive || hoverHighlighted || visualFocus || actionId === "play-toggle" && playbackLoading) ? button.colors.accentText : hoverHighlighted || visualFocus ? button.colors.text : button.colors.iconMain
    scale: player && enabled ? down ? actionId === "play-toggle" ? 0.95 : 0.9 : hovered ? 1.1 : 1 : 1
    Behavior on scale {
        NumberAnimation {
            duration: Theme.normalDuration
        }
    }
    signal aboutToActivate(string id, Item anchor)
    onClicked: {
        aboutToActivate(actionId, button);
        actions.activate(actionKey);
    }
    Shape {
        anchors.centerIn: parent
        width: 16
        height: 16
        visible: button.player && button.actionId === "play-toggle" && button.playbackLoading
        ShapePath {
            fillColor: "transparent"
            strokeColor: button.foreground
            strokeWidth: 2
            PathAngleArc {
                centerX: 8
                centerY: 8
                radiusX: 7
                radiusY: 7
                startAngle: 0
                sweepAngle: 270
            }
        }
        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: 800
            loops: Animation.Infinite
            running: button.visible && button.playbackLoading && Theme.store.motionEnabled
        }
    }
    background: Rectangle {
        radius: button.player && button.actionId === "play-toggle" ? 19 : Theme.controlRadius
        color: button.player && button.actionId === "play-toggle" ? button.playbackLoading || button.lyricStyle && button.hoverHighlighted || !button.lyricStyle && Theme.dark ? button.colors.hover : button.colors.mutedSurface : !button.player && button.hoverHighlighted ? button.colors.hover : "transparent"
        border.width: button.visualFocus ? 2 : 0
        border.color: button.colors.accent
    }
    Keys.onShortcutOverride: event => {
        if ((event.modifiers & Qt.AltModifier) && (event.key === Qt.Key_Left || event.key === Qt.Key_Right))
            event.accepted = true;
    }
    Keys.onPressed: event => {
        if ((event.modifiers & Qt.AltModifier) && (event.key === Qt.Key_Left || event.key === Qt.Key_Right)) {
            actions.shiftAction(actionKey, event.key === Qt.Key_Left ? -1 : 1, projection);
            event.accepted = true;
        }
    }
    EchoBadge {
        objectName: "actionBadge"
        visible: button.badgeVisible
        count: button.actionBadge
        colors: button.colors
        placement: EchoBadge.Floating
        tone: ["quality", "effect", "desktop-lyric", "cast"].includes(button.actionId) ? button.actionBadge === "OFF" ? EchoBadge.Muted : EchoBadge.Accent : EchoBadge.Neutral
    }
}
