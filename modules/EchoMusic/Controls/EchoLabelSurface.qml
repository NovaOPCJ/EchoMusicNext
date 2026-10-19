pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme

// Shared visual surface; public count and tag semantics remain on EchoBadge and EchoTag.
Item {
    id: surface
    anchors.alignWhenCentered: false
    property string text: ""
    property Component contentComponent: null
    property string tone: "neutral"
    property string variant: "soft"
    property color customTone: Theme.accent
    property ThemeColors colors: Theme.colors
    property bool pill: false
    property real radius: Theme.detailRadius
    property bool borderVisible: true
    property font labelFont: Theme.font(small ? 11 : 10, Theme.semiboldWeight)
    property bool small: false
    property bool wrap: false
    property real horizontalPadding: small ? 7 : 5
    property color foreground: variant === "solid" ? "white" : tone === "muted" ? colors.secondaryText : tone === "accent" ? colors.accentText : tone === "custom" ? Qt.rgba(customTone.r * 0.68 + colors.text.r * 0.32, customTone.g * 0.68 + colors.text.g * 0.32, customTone.b * 0.68 + colors.text.b * 0.32, 1) : colors.text
    readonly property color fillColor: variant === "outline" ? "transparent" : variant === "solid" ? (tone === "custom" ? customTone : colors.accent) : tone === "accent" ? colors.selected : tone === "custom" ? Theme.withAlpha(customTone, 0.1) : colors.mutedSurface
    readonly property color ringTone: tone === "custom" ? customTone : colors.accent
    readonly property real ringAlpha: 0.24 + colors.controlBorder.a * 0.76
    readonly property color ringColor: variant === "solid" ? "transparent" : tone === "accent" || tone === "custom" ? Qt.rgba((ringTone.r * 0.24 + colors.controlBorder.r * colors.controlBorder.a * 0.76) / ringAlpha, (ringTone.g * 0.24 + colors.controlBorder.g * colors.controlBorder.a * 0.76) / ringAlpha, (ringTone.b * 0.24 + colors.controlBorder.b * colors.controlBorder.a * 0.76) / ringAlpha, ringAlpha) : colors.strongBorder
    implicitWidth: Math.max(18, (content.active ? content.implicitWidth : label.implicitWidth) + horizontalPadding * 2)
    implicitHeight: wrap && !content.active ? Math.max(small ? 22 : 18, label.contentHeight + 4) : small ? 22 : 18
    Rectangle {
        anchors.fill: parent
        radius: surface.pill ? height / 2 : surface.radius
        color: surface.fillColor
    }
    Rectangle {
        anchors.fill: parent
        radius: surface.pill ? height / 2 : surface.radius
        color: "transparent"
        visible: surface.borderVisible
        border.width: surface.pill ? 0 : 1
        border.color: surface.ringColor
    }
    EchoTextInkMetrics {
        id: ink
        font: label.font
        text: surface.text
        layoutWidth: surface.width
    }
    Text {
        id: label
        width: Math.max(0, surface.width - surface.horizontalPadding * 2)
        anchors.horizontalCenter: parent.horizontalCenter
        height: implicitHeight
        y: surface.wrap ? (surface.height - height) / 2 : ink.hasInk ? surface.height / 2 - baselineOffset - ink.inkCenterFromBaseline : (surface.height - height) / 2
        visible: !content.active
        text: surface.text
        textFormat: Text.PlainText
        color: surface.foreground
        font: surface.labelFont
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignTop
        wrapMode: surface.wrap ? Text.WrapAnywhere : Text.NoWrap
        lineHeightMode: surface.wrap ? Text.FixedHeight : Text.ProportionalHeight
        lineHeight: surface.wrap ? (surface.small ? 11 : 10) * 1.4 : 1
    }
    Loader {
        id: content
        anchors.centerIn: parent
        anchors.alignWhenCentered: false
        active: surface.contentComponent !== null
        sourceComponent: surface.contentComponent
    }
}
