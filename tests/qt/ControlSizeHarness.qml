pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Window {
    id: window
    width: 800
    height: 420
    visible: true
    color: Theme.main
    property int role: EchoControlSize.Standard
    property real surfaceScale: 1
    ColumnLayout {
        x: 24
        y: 24
        width: (window.width - 48) / window.surfaceScale
        scale: window.surfaceScale
        transformOrigin: Item.TopLeft
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 12
            EchoButton {
                objectName: "sizedAction"
                text: "播放 Play"
                glyph: "play"
                size: window.role
            }
            EchoSoftButton {
                objectName: "sizedSoftAction"
                text: "取消 Cancel"
                size: window.role
            }
            EchoTextField {
                objectName: "sizedField"
                Layout.fillWidth: true
                size: window.role
                text: "中文 Latin Text"
                clearable: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: false
            spacing: 12
            EchoSelect {
                objectName: "sizedSelect"
                Layout.fillWidth: true
                size: window.role
                model: ["中文 Latin Option", "第二项"]
            }
            EchoSpinBox {
                objectName: "sizedNumber"
                size: window.role
                from: 0
                to: 100
                value: 50
            }
            EchoDatePicker {
                objectName: "sizedDate"
                Layout.fillWidth: true
                size: window.role
                value: "2000-10-14"
            }
        }
        EchoDrawerHeader {
            objectName: "sizedDrawerHeader"
            Layout.fillWidth: true
            heading: "外观调整 中文 Latin Header"
            description: "标题、正文与关闭入口共用布局"
        }
    }
}
