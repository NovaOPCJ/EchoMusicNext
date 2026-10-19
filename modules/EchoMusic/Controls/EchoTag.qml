pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme

// Status, category and identity tag. Counts use EchoBadge.
Item {
    id: tag
    enum Tone {
        Neutral,
        Muted,
        Accent,
        Custom
    }
    enum Variant {
        Soft,
        Outline,
        Solid
    }
    enum Size {
        ExtraSmall,
        Small
    }
    property string text: ""
    property font font: Theme.font(size === EchoTag.Small ? 11 : 10, Theme.semiboldWeight)
    property real radius: Theme.detailRadius
    property bool borderVisible: true
    // Identity summaries wrap long labels.
    property bool wrap: false
    // Optional content replaces text and must fit the chosen tag height.
    property Component contentComponent: null
    property int tone: EchoTag.Neutral
    property int variant: EchoTag.Soft
    property int size: EchoTag.ExtraSmall
    property color customTone: Theme.accent
    property ThemeColors colors: Theme.colors
    implicitWidth: surface.implicitWidth
    implicitHeight: surface.implicitHeight
    Accessible.role: Accessible.StaticText
    Accessible.name: text
    EchoLabelSurface {
        id: surface
        anchors.fill: parent
        text: tag.text
        labelFont: tag.font
        radius: tag.radius
        borderVisible: tag.borderVisible
        wrap: tag.wrap
        contentComponent: tag.contentComponent
        tone: tag.tone === EchoTag.Custom ? "custom" : tag.tone === EchoTag.Accent ? "accent" : tag.tone === EchoTag.Muted ? "muted" : "neutral"
        variant: tag.variant === EchoTag.Solid ? "solid" : tag.variant === EchoTag.Outline ? "outline" : "soft"
        small: tag.size === EchoTag.Small
        customTone: tag.customTone
        colors: tag.colors
    }
}
