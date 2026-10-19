pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Controls
import EchoMusic.Theme

// Isolated rendering/input fixture; no product data or persistent preferences.
ApplicationWindow {
    id: window
    width: 720
    height: 520
    visible: true
    color: Theme.main
    font: Theme.bodyFont
    property int viewKind: 0
    property int contentPadding: 24
    property real surfaceScale: 1
    readonly property Popup panel: popup
    readonly property Item scrollingView: loader.item as Item
    readonly property QtObject selectPanel: select.popup
    function openSelect(): void {
        select.openPopup();
    }
    readonly property QtObject contextMenu: menu
    EchoSelect {
        id: select
        x: 240
        y: 400
        width: 260
        model: Array.from({
            length: 30
        }, (_, index) => "选项 " + index)
    }
    EchoMenu {
        id: menu
        x: 240
        y: 300
        width: 260
        height: 70
        MenuItem {
            text: "第一项"
        }
        MenuItem {
            text: "第二项"
        }
        MenuItem {
            text: "第三项"
        }
    }
    Flickable {
        id: page
        x: 20
        y: 20
        width: 160
        height: 200
        contentHeight: 2000
        clip: true
        ScrollBar.vertical: EchoPageScrollBar {
            objectName: "pageTrack"
            view: page
        }
    }
    Popup {
        id: popup
        x: 240
        y: 40
        width: 260
        height: 200
        padding: window.contentPadding
        scale: window.surfaceScale
        transformOrigin: Popup.TopLeft
        closePolicy: Popup.NoAutoClose
        background: Rectangle {
            color: Theme.elevated
            radius: Theme.popoverRadius
        }
        contentItem: Loader {
            id: loader
            clip: true
            sourceComponent: window.viewKind === 0 ? flickable : window.viewKind === 1 ? list : scroll
        }
    }
    Component {
        id: flickable
        Flickable {
            id: viewport
            contentHeight: 2000
            clip: true
            ScrollBar.vertical: EchoPopupScrollBar {
                objectName: "popupTrack"
                surface: popup
                view: viewport
                resourceActive: popup.visible
            }
        }
    }
    Component {
        id: list
        ListView {
            id: viewport
            model: 1000
            clip: true
            reuseItems: popup.visible
            cacheBuffer: 0
            delegate: Item {
                width: ListView.view.width
                height: 38
            }
            ScrollBar.vertical: EchoPopupScrollBar {
                objectName: "popupTrack"
                surface: popup
                view: viewport
                resourceActive: popup.visible
            }
        }
    }
    Component {
        id: scroll
        ScrollView {
            id: viewport
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            ScrollBar.vertical: EchoPopupScrollBar {
                objectName: "popupTrack"
                surface: popup
                view: viewport
                resourceActive: popup.visible
            }
            Item {
                width: viewport.availableWidth
                implicitHeight: 2000
            }
        }
    }
}
