pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme

// Test-only shared control matrix, never a product route.
Window {
    id: window
    width: 720
    height: 290
    visible: true
    color: floatingControls ? Theme.elevated : Theme.main
    property bool floatingControls: false
    property real uiScale: 1
    property int buttonHeight: 36
    property int labelSize: 12
    property int verticalPadding: 0
    property string label: "播放 Play"
    property color explicitColor: "#cc3576"
    property bool surfaceProbe: false
    property real surfaceScale: 1
    EchoSoftButton {
        objectName: "accentButton"
        x: 30
        y: 30
        width: 180
        height: window.buttonHeight
        text: window.label
        glyph: "play"
        accent: true
        colors: window.floatingControls ? Theme.floating : Theme.colors
        font: Theme.font(window.labelSize, Theme.semiboldWeight)
        topPadding: window.verticalPadding
        bottomPadding: window.verticalPadding
    }
    EchoSoftButton {
        objectName: "neutralButton"
        x: 240
        y: 30
        width: 180
        height: window.buttonHeight
        text: window.label
        glyph: "list"
        colors: window.floatingControls ? Theme.floating : Theme.colors
        font: Theme.font(window.labelSize, Theme.semiboldWeight)
        topPadding: window.verticalPadding
        bottomPadding: window.verticalPadding
    }
    EchoButton {
        objectName: "ordinaryButton"
        accent: false
        x: 450
        y: 30
        width: 180
        height: window.buttonHeight
        text: window.label
        glyph: "settings"
        colors: window.floatingControls ? Theme.floating : Theme.colors
        font: Theme.font(window.labelSize, Theme.semiboldWeight)
        topPadding: window.verticalPadding
        bottomPadding: window.verticalPadding
    }
    EchoSoftButton {
        objectName: "dangerButton"
        x: 450
        y: 105
        width: 180
        height: 36
        text: "Delete"
        danger: true
        colors: window.floatingControls ? Theme.floating : Theme.colors
    }
    EchoIconButton {
        id: hintButton
        objectName: "iconButton"
        x: 30
        y: 105
        width: 40
        height: 36
        glyph: "heart"
        text: "Favorite"
    }
    Item {
        id: hintPopupAnchor
        x: 500
        y: 185
        width: 24
        height: 24
    }
    EchoAnchoredPopup {
        objectName: "tooltipTestPopup"
        anchorItem: hintPopupAnchor
        modal: false
        focus: false
        preferredWidth: 160
        contentItem: Text {
            text: "Tooltip lifecycle probe"
            color: Theme.floatingText
        }
    }
    EchoIcon {
        objectName: "tintGlyph"
        x: 120
        y: 110
        width: 28
        height: 28
        name: "heart"
        color: window.explicitColor
    }
    EchoIcon {
        objectName: "fixedGlyph"
        x: 180
        y: 110
        width: 28
        height: 28
        name: "sparkles"
        color: "black"
    }
    EchoSelect {
        objectName: "styledSelect"
        x: 240
        y: 105
        width: 180
        colors: window.floatingControls ? Theme.floating : Theme.colors
        model: ["第一项", "第二项"]
    }
    Item {
        anchors.fill: parent
        visible: window.surfaceProbe
        Rectangle {
            anchors.fill: parent
            color: "#557cba"
        }
        Rectangle {
            width: parent.width
            height: 40 + 120 * window.surfaceScale
            color: "#e6849e"
        }
        EchoFloatingSurface {
            objectName: "borderProbeSurface"
            x: 220
            y: 40
            width: 220
            height: 240
            scale: window.surfaceScale
            transformOrigin: Item.TopLeft
        }
    }
    EchoSpinBox {
        objectName: "numberInput"
        x: 30
        y: 220
        width: 180
        from: 0
        to: 9
        value: 7
        suffix: "秒"
    }
}
