pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme

Window {
    id: window
    width: 720
    height: 150
    visible: true
    color: Theme.main
    property real uiScale: 1
    property bool waiting: true
    property var markerModel: null
    Item {
        width: window.width / window.uiScale
        height: window.height / window.uiScale
        scale: window.uiScale
        transformOrigin: Item.TopLeft
        Text {
            x: 24
            y: 22
            text: window.waiting ? "播放进度 · 等待动画预览" : "播放进度 · 已恢复"
            color: Theme.text
            font: Theme.font(13, Theme.semiboldWeight)
        }
        EchoSlider {
            id: slider
            objectName: "previewProgress"
            x: 24
            y: 58
            width: parent.width - 48
            height: 24
            padding: 0
            progress: true
            from: 0
            to: 100
            value: 60
            busy: window.waiting
            markers: window.markerModel
        }
        Text {
            x: 24
            y: 100
            text: window.waiting ? "定位或缓冲时，在已播范围显示扫光" : "恢复播放后回到普通进度条"
            color: Theme.secondaryText
            font: Theme.font(11, Theme.mediumWeight)
        }
    }
}
