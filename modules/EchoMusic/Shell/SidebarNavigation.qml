pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Shell

ColumnLayout {
    id: navigation
    required property SidebarLayoutModel layout
    required property RouteController router
    required property bool collapsed
    spacing: 6
    Repeater {
        // The visible built-in menu excludes entries already moved to cards.
        // The unbounded playlist catalog remains a separate ListView.
        model: navigation.layout.sections.filter(section => section.key !== "playlists")
        delegate: ColumnLayout {
            id: section
            required property var modelData
            Layout.fillWidth: true
            Layout.alignment: navigation.collapsed ? Qt.AlignHCenter : Qt.AlignLeft
            visible: !modelData.hidden
            spacing: navigation.collapsed ? 6 : 2
            Repeater {
                model: section.modelData.items
                delegate: Button {
                    id: entry
                    objectName: "sidebarMenu-" + modelData.key
                    required property var modelData
                    readonly property bool selected: navigation.router.current && navigation.router.current.name === modelData.key
                    Layout.fillWidth: !navigation.collapsed
                    Layout.preferredWidth: navigation.collapsed ? 38 : -1
                    Layout.maximumWidth: navigation.collapsed ? 38 : Infinity
                    Layout.alignment: Qt.AlignHCenter
                    visible: !modelData.hidden
                    implicitHeight: navigation.collapsed ? 38 : 37
                    leftPadding: navigation.collapsed ? 0 : 14
                    rightPadding: leftPadding
                    enabled: navigation.router.available(modelData.key)
                    hoverEnabled: true
                    Accessible.name: modelData.title
                    onClicked: navigation.router.navigate(modelData.key)
                    EchoToolTip {
                        parent: entry
                        text: entry.modelData.title
                        requestedVisible: entry.enabled && entry.hovered && navigation.collapsed
                    }
                    contentItem: EchoButtonContent {
                        spacing: 14
                        opacity: entry.enabled ? 1 : 0.35
                        glyph: entry.modelData.glyph
                        text: navigation.collapsed ? "" : entry.modelData.title
                        glyphSize: navigation.collapsed ? 19 : 18
                        horizontalAlignment: navigation.collapsed ? Qt.AlignHCenter : Qt.AlignLeft
                        color: entry.selected ? Theme.accentText : Theme.text
                        iconColor: entry.selected ? Theme.accentText : entry.hovered && entry.enabled ? Theme.text : Theme.iconMain
                        font: Theme.font(14, entry.selected ? Theme.semiboldWeight : Theme.regularWeight)
                    }
                    background: Rectangle {
                        radius: Theme.itemRadius
                        color: entry.selected ? navigation.collapsed ? Theme.withAlpha(Theme.accent, 0.12) : Theme.selected : entry.hovered && entry.enabled ? navigation.collapsed ? Theme.withAlpha(Theme.text, 0.07) : Theme.rowHover : "transparent"
                    }
                }
            }
        }
    }
}
