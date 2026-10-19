pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Controls
import EchoMusic.Theme

ApplicationWindow {
    id: host
    width: 760
    height: 540
    visible: true
    color: Theme.main
    property var items: [
        {
            label: "Alpha",
            value: 1
        },
        {
            label: "禁用项",
            value: "disabled",
            disabled: true
        },
        {
            label: "Beta 中文长选项名称",
            value: "1"
        }
    ]
    property var selected: undefined
    property bool multi: false
    property bool wrapped: false
    property real zoom: 1
    property int indexSelection: 0
    property alias selector: selector
    EchoBackdropRoot {
        anchors.fill: parent
        EchoSelect {
            objectName: "indexBoundSelect"
            x: 24
            y: 24
            width: 180
            model: ["One", "Blocked", "Three"]
            availableOptions: [true, false, true]
            currentIndex: host.indexSelection
        }
        EchoSelect {
            id: selector
            objectName: "featureSelect"
            x: 400
            y: 360
            width: 180
            scale: host.zoom
            transformOrigin: Item.TopLeft
            options: host.items
            value: host.selected
            multiple: host.multi
            filterable: true
            clearable: true
            wrapOptions: host.wrapped
            onValueEdited: nextValue => host.selected = nextValue
        }
    }
}
