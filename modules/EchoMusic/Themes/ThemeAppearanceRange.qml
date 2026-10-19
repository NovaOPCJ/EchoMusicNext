pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme
import EchoMusic.Controls

ColumnLayout {
    id: field
    required property string title
    required property real value
    property real minimum: 0
    property real maximum: 100
    property real step: 1
    signal moved(real nextValue)
    spacing: 7
    RowLayout {
        Layout.fillHeight: false
        spacing: 5
        Layout.fillWidth: true
        Label {
            text: field.title
            color: Theme.floatingText
            font: Theme.font(12, Theme.regularWeight)
            Layout.fillWidth: true
        }
        Label {
            text: Math.round(field.value) + "%"
            color: Theme.floatingSecondaryText
            font: Theme.font(12, Theme.regularWeight)
        }
    }
    EchoSlider {
        Layout.fillWidth: true
        colors: Theme.floating
        from: field.minimum
        to: field.maximum
        stepSize: field.step
        value: field.value
        Accessible.name: field.title
        onMoved: field.moved(value)
    }
}
