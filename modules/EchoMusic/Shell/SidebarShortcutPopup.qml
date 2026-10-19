pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Shell

EchoDialog {
    id: popup
    objectName: "sidebarShortcutPopup"
    required property SidebarLayoutModel layout
    required property RouteController router
    required property Item anchorItem
    readonly property rect anchorRect: geometry.rect
    readonly property real sidebarEdge: anchorRect.x + anchorRect.width
    readonly property real edge: 12 * uiScale
    heading: qsTr("添加卡片")
    preferredWidth: 520
    maximumHeight: parent ? parent.height / uiScale - 24 : 620
    windowHeightMargin: 24
    modal: false
    // Anchor against the whole sidebar rather than the add button.
    x: parent && parent.width - sidebarEdge - 20 * uiScale >= 340 * uiScale ? sidebarEdge + 8 * uiScale : edge
    width: parent ? Math.max(0, Math.min(520, (parent.width - x) / uiScale - 12)) : 520
    height: maximumHeight
    y: edge
    EchoPopupAnchor {
        id: geometry
        item: popup.anchorItem
        coordinateItem: popup.parent
    }
    flushBody: true
    body: Component {
        Flickable {
            id: shortcutView
            implicitHeight: popup.bodyHeightLimit
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            contentWidth: width
            contentHeight: body.implicitHeight
            ScrollBar.vertical: EchoPopupScrollBar {
                surface: popup
                view: shortcutView
                resourceActive: popup.visible
            }
            ColumnLayout {
                id: body
                width: shortcutView.width - 24
                spacing: 0
                Label {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 16
                    text: qsTr("拖动卡片调整顺序，第一张就是首页；至少保留一张卡片。")
                    color: Theme.floatingSecondaryText
                    font: Theme.font(12, Theme.regularWeight)
                    lineHeightMode: Text.FixedHeight
                    lineHeight: 19.2
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    Layout.fillHeight: false
                    spacing: 5
                    Layout.fillWidth: true
                    Layout.bottomMargin: 16
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("常用功能")
                        color: Theme.floatingText
                        font: Theme.font(17, Theme.semiboldWeight)
                    }
                    EchoSoftButton {
                        text: qsTr("恢复默认")
                        colors: Theme.floating
                        implicitHeight: 28
                        font: Theme.font(12, Theme.semiboldWeight)
                        onClicked: popup.layout.restoreShortcuts()
                    }
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10
                    Repeater {
                        // Nine fixed function entries, no resource catalog here.
                        model: popup.layout.shortcutCatalog
                        delegate: Button {
                            id: option
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            implicitHeight: Math.max(23, label.implicitHeight) + 30
                            padding: 15
                            enabled: modelData.pinned ? popup.layout.shortcuts.length > 1 : popup.router.available(modelData.route)
                            hoverEnabled: true
                            Accessible.name: (modelData.pinned ? qsTr("移除") : qsTr("添加")) + modelData.title
                            Accessible.checkable: true
                            Accessible.checked: modelData.pinned
                            onClicked: popup.layout.toggleShortcut(modelData.key)
                            background: Rectangle {
                                radius: Theme.controlRadius
                                color: option.hovered && option.enabled ? Theme.floating.hover : Theme.floating.mutedSurface
                            }
                            contentItem: RowLayout {
                                spacing: 12
                                EchoIcon {
                                    name: option.modelData.glyph
                                    Layout.preferredWidth: 23
                                    Layout.preferredHeight: 23
                                    color: option.modelData.pinned || !option.enabled ? Theme.floatingSecondaryText : Theme.floatingText
                                }
                                EchoTextTagRow {
                                    id: label
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    text: option.modelData.title
                                    wrap: true
                                    fillTextWidth: true
                                    color: option.modelData.pinned || !option.enabled ? Theme.floatingSecondaryText : Theme.floatingText
                                    font: Theme.font(14, Theme.regularWeight)
                                }
                                EchoIcon {
                                    name: option.modelData.pinned ? "minus" : "plus"
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    color: option.modelData.pinned || !option.enabled ? Theme.floatingSecondaryText : Theme.floatingText
                                }
                            }
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 30
                    spacing: 16
                    Label {
                        objectName: "shortcutArtistsHeading"
                        text: qsTr("常听艺人")
                        color: Theme.floatingText
                        font: Theme.font(17, Theme.semiboldWeight)
                    }
                    Label {
                        objectName: "shortcutArtistsEmpty"
                        Layout.fillWidth: true
                        Layout.topMargin: 18
                        Layout.bottomMargin: 18
                        text: qsTr("播放歌曲后，常听艺人会出现在这里")
                        wrapMode: Text.Wrap
                        color: Theme.floatingSecondaryText
                        font: Theme.font(13, Theme.regularWeight)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 30
                    spacing: 16
                    Label {
                        objectName: "shortcutPlaylistsHeading"
                        text: qsTr("最近常听")
                        color: Theme.floatingText
                        font: Theme.font(17, Theme.semiboldWeight)
                    }
                    Label {
                        objectName: "shortcutPlaylistsEmpty"
                        Layout.fillWidth: true
                        Layout.topMargin: 18
                        Layout.bottomMargin: 18
                        text: qsTr("播放过的歌单会出现在这里")
                        wrapMode: Text.Wrap
                        color: Theme.floatingSecondaryText
                        font: Theme.font(13, Theme.regularWeight)
                    }
                }
            }
        }
    }
}
