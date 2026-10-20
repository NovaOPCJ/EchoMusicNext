pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme

ToolButton {
    id: control
    required property string glyph
    // Pure icon actions expose their label on hover as well as to accessibility.
    property string tooltipText: text
    property bool animatePlayPause: false
    // Cover presentation without replacing the semantic glyph/state.
    property bool iconCovered: false
    property real playIconOffset: 0
    property ThemeColors colors: Theme.colors
    property bool hoverBackground: true
    property bool hoverWhenDisabled: false
    // Qt 6 delivers hover to disabled controls. A locally unavailable titlebar
    // action can highlight, while an entire disabled overlay subtree cannot.
    readonly property bool hoverHighlighted: hovered && (enabled || hoverWhenDisabled && parent !== null && parent.enabled)
    property color foreground: hoverHighlighted || visualFocus ? colors.text : colors.iconMain
    property real disabledOpacity: 0.6
    implicitWidth: 34
    implicitHeight: 34
    hoverEnabled: true
    Accessible.name: text
    Loader {
        // This is only a trigger recipe. Native hints are still created after
        // the hover delay and destroyed when the interaction ends.
        active: control.tooltipText.length > 0
        sourceComponent: EchoToolTip {
            objectName: "iconButtonToolTip"
            parent: control
            requestedVisible: control.hovered && control.enabled && control.tooltipText.length > 0
            text: control.tooltipText
        }
    }
    contentItem: Loader {
        visible: !control.iconCovered
        opacity: control.enabled ? 1 : control.disabledOpacity
        sourceComponent: control.animatePlayPause ? animatedIcon : staticIcon
    }
    Component {
        id: staticIcon
        EchoIcon {
            name: control.glyph
            color: control.foreground
        }
    }
    Component {
        id: animatedIcon
        EchoPlayPauseIcon {
            playing: control.glyph === "pause"
            covered: control.iconCovered
            color: control.foreground
            playOffset: control.playIconOffset
            visible: control.glyph === "play" || control.glyph === "pause"
        }
    }
    padding: 7
    background: Rectangle {
        radius: Theme.controlRadius
        color: control.hoverBackground && control.hoverHighlighted ? control.colors.hover : "transparent"
        border.width: control.visualFocus ? 2 : 0
        border.color: Theme.accent
    }
}
