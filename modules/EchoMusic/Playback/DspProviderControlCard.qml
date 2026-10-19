pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Playback

// Trusted provider-manifest presentation. Validation belongs to the Qt service;
// slider previews are local until release or keyboard commit.
Rectangle {
    id: card
    required property var modelData
    required property PlaybackAudioControls audio
    objectName: "dspProviderControl-" + modelData.id
    readonly property bool editable: modelData.ownership === "provider" && !audio.effectPending
    readonly property var options: modelData.options || []
    readonly property var range: modelData.range || {}
    readonly property var option: options.find(o => JSON.stringify(o.value) === JSON.stringify(modelData.value))
    readonly property string valueLabel: option ? option.label : typeof modelData.value === "boolean" ? modelData.value ? qsTr("开启") : qsTr("关闭") : String(modelData.value)
    function booleanLabel(value: bool): string {
        const option = options.find(o => o.value === value);
        return option ? option.label : value ? qsTr("开启") : qsTr("关闭");
    }
    visible: modelData.shown
    implicitHeight: visible ? content.implicitHeight + 24 : 0
    radius: Theme.controlRadius
    color: Theme.floating.mutedSurface
    opacity: editable ? 1 : 0.42
    ColumnLayout {
        id: content
        x: 12
        y: 12
        width: card.width - 24
        spacing: 12
        // Bound both shaped text boxes to the already known card width.
        // A RowLayout whose child's maximumWidth reads that layout's width
        // feeds a changing constraint back into the layout during recycling.
        Item {
            id: heading
            Layout.fillWidth: true
            implicitHeight: 20
            EchoTextTagRow {
                objectName: "dspControlLabel"
                width: Math.max(0, heading.width - valueText.width - 10)
                height: heading.height
                text: card.modelData.label
                font: Theme.font(13, Theme.boldWeight)
                lineHeight: 20
                color: Theme.floating.text
            }
            EchoTextTagRow {
                id: valueText
                objectName: "dspControlValue"
                x: heading.width - width
                width: Math.min(Math.ceil(implicitWidth), heading.width * .45)
                height: heading.height
                horizontalAlignment: Text.AlignRight
                text: card.modelData.type === "number" && number.dirty ? String(number.value) + (card.modelData.unit || "") : card.valueLabel + (card.modelData.unit || "")
                font: Theme.font(12, Theme.regularWeight)
                lineHeight: 20
                color: Theme.floating.secondaryText
            }
        }
        ColumnLayout {
            visible: card.modelData.type === "number"
            Layout.fillWidth: true
            spacing: 4
            EchoSlider {
                id: number
                Layout.fillWidth: true
                colors: Theme.floating
                from: card.range.inverted ? card.range.max : card.range.min || 0
                to: card.range.inverted ? card.range.min : card.range.max === undefined ? 1 : card.range.max
                stepSize: card.range.step === undefined ? 0.01 : card.range.step
                enabled: card.editable
                Accessible.name: card.modelData.label
                property bool dirty: false
                Binding on value {
                    when: !number.pressed
                    value: card.modelData.type === "number" ? Number(card.modelData.value) : number.from
                    restoreMode: Binding.RestoreNone
                }
                onMoved: {
                    if (pressed)
                        dirty = true;
                    else
                        card.audio.setProviderControl(card.modelData.id, value);
                }
                onPressedChanged: if (!pressed && dirty) {
                    dirty = false;
                    card.audio.setProviderControl(card.modelData.id, value);
                }
            }
            RowLayout {
                Layout.fillHeight: false
                spacing: 5
                visible: !!card.range.minLabel || !!card.range.maxLabel
                Layout.fillWidth: true
                Label {
                    text: (card.range.inverted ? card.range.maxLabel : card.range.minLabel) || ""
                    font: Theme.font(11, Theme.regularWeight)
                    color: Theme.floating.secondaryText
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    text: (card.range.inverted ? card.range.minLabel : card.range.maxLabel) || ""
                    font: Theme.font(11, Theme.regularWeight)
                    color: Theme.floating.secondaryText
                }
            }
        }
        RowLayout {
            Layout.fillHeight: false
            visible: card.modelData.type === "boolean"
            spacing: 8
            EchoTextTagRow {
                objectName: "dspBooleanOffLabel"
                text: card.booleanLabel(false)
                font: Theme.font(12, Theme.semiboldWeight)
                lineHeight: 24
                color: Theme.floating.secondaryText
            }
            EchoSwitch {
                objectName: "dspBooleanSwitch"
                Layout.fillWidth: false
                Layout.fillHeight: false
                Layout.maximumWidth: implicitWidth
                colors: Theme.floating
                checked: card.modelData.value === true
                enabled: card.editable && (card.options.length === 0 || card.options.some(o => o.value === !checked && !o.disabled))
                Accessible.name: card.modelData.label
                onToggled: card.audio.setProviderControl(card.modelData.id, checked)
            }
            EchoTextTagRow {
                objectName: "dspBooleanOnLabel"
                text: card.booleanLabel(true)
                font: Theme.font(12, Theme.semiboldWeight)
                lineHeight: 24
                color: Theme.floating.secondaryText
            }
        }
        EchoSelect {
            visible: card.modelData.type === "select"
            Layout.fillWidth: true
            font: Theme.font(12, Theme.semiboldWeight)
            colors: Theme.floating
            options: card.options
            value: card.modelData.value
            enabled: card.editable
            Accessible.name: card.modelData.label
            onValueEdited: nextValue => card.audio.setProviderControl(card.modelData.id, nextValue)
        }
        Label {
            visible: !!card.modelData.description
            Layout.fillWidth: true
            text: card.modelData.description || ""
            font: Theme.font(11, Theme.regularWeight)
            color: Theme.floating.secondaryText
            wrapMode: Text.Wrap
            lineHeightMode: Text.ProportionalHeight
            lineHeight: 1.5
        }
    }
}
