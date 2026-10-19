pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Item {
    id: header
    required property ExploreService service
    property bool showNavigation: true
    property bool showToolbar: true
    property real inset: 40
    signal pickRequested(string kind)
    signal batchRequested
    implicitHeight: (showNavigation ? 96 : 0) + (showToolbar ? 60 : 0)
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: header.inset
        anchors.rightMargin: header.inset
        spacing: 0
        Label {
            visible: header.showNavigation
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            text: qsTr("探索发现")
            font: Theme.spacedFont(24, Theme.semiboldWeight, -.6)
            color: Theme.text
            verticalAlignment: Text.AlignVCenter
        }
        EchoTabBar {
            objectName: "exploreNavigationTabs"
            visible: header.showNavigation
            Layout.fillWidth: true
            Layout.topMargin: 16
            labels: [qsTr("歌单"), qsTr("排行榜"), qsTr("新碟上架"), qsTr("新歌速递"), qsTr("歌手")]
            currentIndex: header.service.tab
            onPicked: index => header.service.selectTab(index)
            accessibleName: qsTr("探索分类")
        }
        RowLayout {
            objectName: "exploreFilterRow"
            visible: header.showToolbar
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.fillHeight: false
            spacing: 12
            EchoSoftButton {
                id: categorySelector
                objectName: "exploreCategorySelector"
                visible: header.service.tab !== 3 && header.service.tab !== 4
                text: header.service.tab === 0 ? header.service.categoryLabel : header.service.tab === 1 ? header.service.rankLabel : ({
                        all: qsTr("全部"),
                        chn: qsTr("华语"),
                        eur: qsTr("欧美"),
                        jpn: qsTr("日本"),
                        kor: qsTr("韩国")
                    })[header.service.region]
                font: Theme.font(13, Theme.semiboldWeight)
                contentItem: EchoButtonContent {
                    text: categorySelector.text
                    font: categorySelector.font
                    color: Theme.text
                    iconColor: Theme.secondaryText
                    glyph: "chevron-down"
                    glyphSize: 14
                    glyphAfterText: true
                }
                onClicked: header.pickRequested(header.service.tab === 0 ? "category" : header.service.tab === 1 ? "rank" : "region")
            }
            EchoSoftButton {
                id: sexSelector
                objectName: "exploreSexSelector"
                visible: header.service.tab === 4
                text: qsTr("性别: %1").arg(header.service.sexFilters.find(option => option.id === header.service.sex)?.label || qsTr("全部"))
                font: Theme.font(13, Theme.semiboldWeight)
                contentItem: EchoButtonContent {
                    text: sexSelector.text
                    font: sexSelector.font
                    color: Theme.text
                    iconColor: Theme.secondaryText
                    glyph: "chevron-down"
                    glyphSize: 14
                    glyphAfterText: true
                }
                onClicked: header.pickRequested("sex")
            }
            EchoSoftButton {
                id: typeSelector
                visible: header.service.tab === 4
                text: qsTr("类型: %1").arg(header.service.typeFilters.find(option => option.id === header.service.artistType)?.label || qsTr("全部"))
                font: Theme.font(13, Theme.semiboldWeight)
                contentItem: EchoButtonContent {
                    text: typeSelector.text
                    font: typeSelector.font
                    color: Theme.text
                    iconColor: Theme.secondaryText
                    glyph: "chevron-down"
                    glyphSize: 14
                    glyphAfterText: true
                }
                onClicked: header.pickRequested("artistType")
            }
            RowLayout {
                objectName: "exploreNewSongsTitle"
                visible: header.service.tab === 3
                spacing: 10
                Rectangle {
                    implicitWidth: 32
                    implicitHeight: 32
                    radius: Theme.controlRadius
                    color: Theme.withAlpha(Theme.accent, .10)
                    EchoIcon {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        name: "sparkles"
                        color: Theme.accentText
                    }
                }
                EchoTextTagRow {
                    text: qsTr("新歌速递")
                    font: Theme.font(15, Theme.semiboldWeight)
                    color: Theme.text
                }
            }
            Item {
                Layout.fillWidth: true
            }
            EchoSoftButton {
                objectName: "explorePlayAll"
                visible: header.service.tab === 1 || header.service.tab === 3
                text: qsTr("播放")
                glyph: "play"
                accent: true
                enabled: (header.service.tab === 1 ? header.service.rankSongs : header.service.newSongs).count > 0
                onClicked: (header.service.tab === 1 ? header.service.rankActions : header.service.newActions).play()
            }
            EchoSoftButton {
                visible: header.service.tab === 1 || header.service.tab === 3
                text: qsTr("批量")
                glyph: "list"
                enabled: (header.service.tab === 1 ? header.service.rankSongs : header.service.newSongs).count > 0
                onClicked: header.batchRequested()
            }
        }
    }
}
