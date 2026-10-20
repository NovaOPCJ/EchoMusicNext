pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme

// Plugin actions use standard button geometry and shared interaction states.
EchoSoftButton {
    id: button
    property bool iconOnly: false
    property string tooltipText: iconOnly ? text : ""
    implicitHeight: EchoControlSize.heightFor(size)
    implicitWidth: iconOnly ? 36 : Math.ceil(contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: iconOnly ? 0 : EchoControlSize.buttonPaddingFor(size)
    rightPadding: leftPadding
    font: Theme.font(12, Theme.semiboldWeight)
    // The spinner, optional glyph and label share one intrinsic centered row.
    // An icon-only loading action must not reserve an empty trailing label slot.
    contentItem: EchoButtonContent {
        text: button.iconOnly ? "" : button.text
        glyph: button.loading ? "" : button.glyph
        loading: button.loading
        glyphSize: button.iconOnly ? 16 : 14
        spacing: 8
        color: button.foreground
        font: button.font
    }
    EchoToolTip {
        parent: button
        requestedVisible: button.hovered && button.enabled && button.tooltipText.length > 0
        text: button.tooltipText
    }
}
