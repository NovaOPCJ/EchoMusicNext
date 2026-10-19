pragma ComponentBehavior: Bound
import QtQuick
import QtQml
import QtQuick.Controls.Basic
import EchoMusic.Theme

// Editable decimal field with optional suffix and stacked controls.
// Qt DoubleSpinBox owns clamping, keyboard input and held-button repeat;
// decimals=0 retains integer behavior.
DoubleSpinBox {
    id: control
    decimals: 0
    textFromValue: (value, decimals, locale) => {
        const numbers = Qt.locale(locale.name);
        numbers.numberOptions = locale.numberOptions | Locale.OmitGroupSeparator;
        // Display plain digits without grouped thousands.
        let text = Number(value).toLocaleString(numbers, 'f', decimals);
        if (decimals > 0) {
            text = text.replace(/0+$/, "");
            if (text.endsWith(locale.decimalPoint))
                text = text.slice(0, -locale.decimalPoint.length);
        }
        return text;
    }
    valueFromText: (text, locale) => Number.fromLocaleString(locale, text)
    property ThemeColors colors: Theme.colors
    property string suffix: ""
    property font suffixFont: Theme.font(13, Theme.semiboldWeight)
    signal accepted
    function commit(): void {
        if (!enabled)
            return;
        const previous = value;
        const next = valueFromText(input.text, locale);
        if (Number.isFinite(next))
            value = Math.max(Math.min(from, to), Math.min(Math.max(from, to), Math.round(next * Math.pow(10, decimals)) / Math.pow(10, decimals)));
        // Restore the display binding after accepting an editable draft.
        input.text = Qt.binding(() => control.textFromValue(control.value, control.decimals, control.locale));
        if (value !== previous)
            valueModified();
    }
    property int size: EchoControlSize.Standard
    readonly property bool canIncrease: enabled && (wrap || (from < to ? value < to : value > to))
    readonly property bool canDecrease: enabled && (wrap || (from < to ? value > from : value < from))
    readonly property real indicatorWidth: size === EchoControlSize.Small ? 22 : 28
    implicitWidth: 110
    implicitHeight: EchoControlSize.heightFor(size)
    editable: true
    hoverEnabled: true
    opacity: enabled ? 1 : .5
    leftPadding: EchoControlSize.fieldPaddingFor(size)
    rightPadding: indicatorWidth + (suffix.length ? unit.implicitWidth + 8 : 0)
    font: Theme.font(size === EchoControlSize.Large ? 15 : size === EchoControlSize.Standard ? 13 : 12, Theme.mediumWeight)
    contentItem: TextInput {
        id: input
        objectName: "numberInputField"
        selectByMouse: true
        // Qt commits and emits valueModified for Return; consumers read after it.
        onAccepted: Qt.callLater(() => control.accepted())
        text: control.textFromValue(control.value, control.decimals, control.locale)
        color: control.colors.text
        selectionColor: control.colors.accent
        selectedTextColor: control.colors.accentText
        font: control.font
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        clip: true
    }
    Label {
        id: unit
        visible: control.suffix.length > 0
        text: control.suffix
        x: control.width - control.indicatorWidth - width - 8
        height: control.height
        verticalAlignment: Text.AlignVCenter
        font: control.suffixFont
        color: control.colors.secondaryText
    }
    background: Rectangle {
        color: control.colors.mutedSurface
        radius: Theme.controlRadius
        border.width: 1
        border.color: control.activeFocus ? Theme.mix(control.colors.controlBorder, control.colors.accent, .35) : control.colors.controlBorder
    }
    up.indicator: Rectangle {
        x: control.width - width - 1
        y: 1
        width: control.indicatorWidth - 1
        height: control.height / 2 - 1
        topRightRadius: Theme.controlRadius - 1
        color: !control.canIncrease ? "transparent" : control.up.pressed ? control.colors.neutralPressed : control.up.hovered ? control.colors.hover : "transparent"
        Rectangle {
            width: 1
            height: parent.height
            color: control.colors.controlBorder
        }
        EchoIcon {
            anchors.centerIn: parent
            width: 12
            height: 12
            name: "chevron-up"
            color: control.colors.iconMain
            opacity: control.canIncrease ? 1 : .3
        }
    }
    down.indicator: Rectangle {
        x: control.width - width - 1
        y: control.height / 2
        width: control.indicatorWidth - 1
        height: control.height / 2 - 1
        bottomRightRadius: Theme.controlRadius - 1
        color: !control.canDecrease ? "transparent" : control.down.pressed ? control.colors.neutralPressed : control.down.hovered ? control.colors.hover : "transparent"
        Rectangle {
            width: 1
            height: parent.height
            color: control.colors.controlBorder
        }
        Rectangle {
            width: parent.width
            height: 1
            color: control.colors.controlBorder
        }
        EchoIcon {
            anchors.centerIn: parent
            width: 12
            height: 12
            name: "chevron-down"
            color: control.colors.iconMain
            opacity: control.canDecrease ? 1 : .3
        }
    }
}
