pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme
import EchoMusic.Controls
import EchoMusic.Playback

EchoDrawer {
    id: drawer
    objectName: "lyricSkinDrawer"
    required property PlaybackService playback
    property bool showSettings: false
    signal globalSettingsRequested
    preferredWidth: 360
    playerBottomInset: 88
    onAboutToHide: skinSettings.closePopups()
    function openSkins(): void {
        showSettings = false;
        open();
    }
    contentItem: ColumnLayout {
        spacing: 0
        EchoDrawerHeader {
            Layout.fillWidth: true
            heading: drawer.showSettings ? qsTr("皮肤设置") : qsTr("换肤")
            description: drawer.showSettings ? qsTr("封面歌词") : ""
            headingFont: Theme.font(16, 800)
            closeLabel: qsTr("关闭皮肤设置")
            onCloseRequested: drawer.close()
            actions: Component {
                RowLayout {
                    spacing: 8
                    EchoIconButton {
                        visible: drawer.showSettings
                        Layout.fillHeight: false
                        implicitWidth: 32
                        implicitHeight: 32
                        colors: Theme.floating
                        glyph: "chevron-left"
                        text: qsTr("返回换肤")
                        onClicked: drawer.showSettings = false
                    }
                    EchoIconButton {
                        Layout.fillHeight: false
                        implicitWidth: 32
                        implicitHeight: 32
                        colors: Theme.floating
                        glyph: "settings"
                        text: qsTr("全局设置")
                        onClicked: {
                            drawer.close();
                            drawer.globalSettingsRequested();
                        }
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.floatingBorder
        }
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            contentWidth: availableWidth
            clip: true
            ScrollBar.vertical: EchoPopupScrollBar {
                surface: drawer
                view: scroll
                resourceActive: drawer.visible
            }
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 12
                ColumnLayout {
                    visible: !drawer.showSettings
                    Layout.fillWidth: true
                    Layout.margins: 16
                    Layout.bottomMargin: 24
                    spacing: 8
                    EchoTextTagRow {
                        text: qsTr("内置")
                        font: Theme.spacedFont(11, Theme.boldWeight, .66)
                        color: Theme.floatingSecondaryText
                    }
                    Button {
                        id: card
                        objectName: "coverLyricSkinCard"
                        Layout.preferredWidth: Math.max(120, (scroll.availableWidth - 44) / 2)
                        implicitHeight: (width - 22) * 9 / 16 + 7 + 19.5 + 22
                        padding: 11
                        hoverEnabled: true
                        Accessible.name: qsTr("封面歌词，已选中，点击打开皮肤设置")
                        Accessible.checked: true
                        onClicked: drawer.showSettings = true
                        background: Rectangle {
                            radius: Theme.cardRadius
                            color: card.hovered ? Theme.floating.hover : Theme.floatingCard
                            border.width: 1
                            border.color: Theme.accent
                        }
                        contentItem: ColumnLayout {
                            spacing: 7
                            Rectangle {
                                objectName: "coverSkinPreview"
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                radius: Theme.controlRadius
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop {
                                        position: 0
                                        color: "#1b1f27"
                                    }
                                    GradientStop {
                                        position: 1
                                        color: "#101216"
                                    }
                                }
                                Rectangle {
                                    x: 12
                                    y: (parent.height - height) / 2
                                    width: 34
                                    height: width
                                    radius: Theme.mediaRadius
                                    gradient: Gradient {
                                        GradientStop {
                                            position: 0
                                            color: "#5b8def"
                                        }
                                        GradientStop {
                                            position: .55
                                            color: "#8b5cf6"
                                        }
                                        GradientStop {
                                            position: 1
                                            color: "#ec4899"
                                        }
                                    }
                                }
                                Column {
                                    x: 56
                                    y: (parent.height - height) / 2
                                    width: parent.width - 68
                                    spacing: 5
                                    Repeater {
                                        model: [.8, .55, .65]
                                        Rectangle {
                                            required property real modelData
                                            width: parent.width * modelData
                                            height: 4
                                            radius: Theme.microRadius
                                            color: Theme.withAlpha("white", modelData === .55 ? .4 : .22)
                                        }
                                    }
                                }
                                EchoSelectionBadge {
                                    objectName: "coverSkinSelectionBadge"
                                    z: 2
                                    visible: !card.hovered
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 8
                                }
                                Rectangle {
                                    z: 2
                                    visible: card.hovered
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 8
                                    width: 20
                                    height: 20
                                    radius: Theme.detailRadius
                                    color: Theme.elevated
                                    Rectangle {
                                        anchors.fill: parent
                                        radius: parent.radius
                                        color: Theme.selected
                                    }
                                    EchoIcon {
                                        anchors.centerIn: parent
                                        width: 14
                                        height: 14
                                        name: "settings"
                                        color: Theme.accentText
                                    }
                                }
                            }
                            EchoTextTagRow {
                                Layout.fillWidth: true
                                text: qsTr("封面歌词")
                                font: Theme.font(13, Theme.semiboldWeight)
                                lineHeight: 19.5
                                color: Theme.floatingText
                            }
                        }
                    }
                }
                LyricSkinSettings {
                    id: skinSettings
                    visible: drawer.showSettings
                    Layout.fillWidth: true
                    Layout.margins: 16
                    Layout.bottomMargin: 24
                    appearance: drawer.playback.lyricAppearance
                    coverColor: Theme.store.lyricAccent(drawer.playback.coverColor)
                    uiScale: drawer.uiScale
                }
            }
        }
    }
}
