pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

// Shared layout contract only, never registered as an application route.
Window {
    id: window
    width: 850
    height: 720
    visible: true
    color: Theme.main
    property real surfaceScale: 1
    property bool withActions: false
    property bool showDrawer: false
    property real drawerWidth: 440
    property bool drawerLoading: false
    property bool secondaryAction: false
    property bool flushBody: false
    property bool closable: true
    property real topInset: 24
    property string heading: "切换账号"
    readonly property alias panel: panel
    EchoDialog {
        id: panel
        uiScale: window.surfaceScale
        preferredWidth: 440
        heading: window.heading
        headingCount: 3
        closable: window.closable
        flushBody: window.flushBody
        topPadding: window.topInset
        headerActions: window.withActions ? refresh : null
        body: Item {
            implicitHeight: 80
            Rectangle {
                objectName: "dialogBodyContentProbe"
                anchors.fill: parent
                anchors.rightMargin: window.flushBody ? 24 : 0
                color: Theme.floating.mutedSurface
            }
        }
        footerContent: Item {
            implicitHeight: 32
            Rectangle {
                anchors.fill: parent
                color: Theme.floating.mutedSurface
            }
        }
    }
    EchoDrawerHeader {
        objectName: "allocationDrawerHeader"
        visible: window.showDrawer
        x: 24
        y: 24
        width: window.drawerWidth
        scale: window.surfaceScale
        transformOrigin: Item.TopLeft
        heading: window.heading
        description: "说明文字 Description 随标题保留自然高度"
        leadingContent: Component {
            EchoIcon {
                implicitWidth: 24
                implicitHeight: 24
                name: "palette"
                color: Theme.floatingText
            }
        }
        actions: window.withActions ? drawerActions : null
    }
    Component {
        id: drawerActions
        RowLayout {
            spacing: 8
            EchoButton {
                objectName: "allocationDrawerRefresh"
                size: EchoControlSize.Compact
                text: "刷新 Update"
                glyph: "refresh"
                loading: window.drawerLoading
                colors: Theme.floating
            }
            EchoIconButton {
                objectName: "allocationDrawerSecondary"
                visible: window.secondaryAction
                implicitWidth: 32
                implicitHeight: 32
                glyph: "folder-open"
                text: "打开目录"
                colors: Theme.floating
            }
        }
    }
    Component {
        id: refresh
        EchoSoftButton {
            implicitHeight: 32
            text: "刷新"
            colors: Theme.floating
        }
    }
}
