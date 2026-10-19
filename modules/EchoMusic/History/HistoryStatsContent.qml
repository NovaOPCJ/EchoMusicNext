pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

// One visual implementation for the live page and demand-created image export.
ColumnLayout {
    id: statsColumn
    required property real viewportWidth
    readonly property bool compactViewport: viewportWidth <= 640
    readonly property bool stackedPanels: viewportWidth <= 980
    property var stats: ({})
    property bool resourceActive: true
    property bool animate: true
    readonly property int pendingImages: {
        let count = 0;
        for (let i = 0; i < songRepeater.count; ++i) {
            const row = songRepeater.itemAt(i) as TopSongRow;
            if (row && row.coverStatus === Image.Loading)
                ++count;
        }
        return count;
    }
    spacing: 16
    GridLayout {
        objectName: "historyStatGrid"
        Layout.fillWidth: true
        columns: statsColumn.compactViewport ? 1 : statsColumn.stackedPanels ? 2 : 4
        columnSpacing: 12
        rowSpacing: 12
        Repeater {
            model: statsColumn.stats.statCards || []
            delegate: Rectangle {
                id: statCard
                required property var modelData
                required property int index
                objectName: "historyStatCard"
                Layout.fillWidth: true
                implicitHeight: 112
                radius: Theme.popoverRadius
                color: Theme.mutedSurface
                border.width: 1
                border.color: Theme.subtleBorder
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 38
                        Layout.preferredHeight: 38
                        radius: Theme.cardRadius
                        color: Theme.withAlpha(statCard.modelData.tone === "cyan" ? "#0891b2" : statCard.modelData.tone === "emerald" ? "#059669" : statCard.modelData.tone === "amber" ? "#d97706" : Theme.accent, .12)
                        EchoIcon {
                            anchors.centerIn: parent
                            width: 18
                            height: 18
                            name: statCard.modelData.icon
                            color: statCard.modelData.tone === "cyan" ? "#0891b2" : statCard.modelData.tone === "emerald" ? "#059669" : statCard.modelData.tone === "amber" ? "#d97706" : Theme.accentText
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 0
                        EchoTextTagRow {
                            Layout.fillWidth: true
                            text: statCard.modelData.label
                            color: Theme.secondaryText
                            font: Theme.font(12, Theme.boldWeight)
                            lineHeight: 16
                        }
                        EchoTextTagRow {
                            Layout.fillWidth: true
                            Layout.topMargin: 4
                            text: statCard.modelData.value
                            color: Theme.text
                            font: Theme.font(24, Theme.extraBoldWeight)
                            lineHeight: 28
                        }
                        EchoTextTagRow {
                            Layout.fillWidth: true
                            Layout.topMargin: 6
                            text: statCard.modelData.caption
                            color: Theme.secondaryText
                            font: Theme.font(12, Theme.semiboldWeight)
                            lineHeight: 18
                        }
                    }
                }
                EchoEnter {
                    target: statCard
                    active: statsColumn.resourceActive
                    duration: statsColumn.animate ? Theme.normalDuration : 0
                    order: statCard.index
                }
            }
        }
    }
    GridLayout {
        id: activityGrid
        objectName: "historyActivityGrid"
        Layout.fillWidth: true
        columns: statsColumn.stackedPanels ? 1 : 2
        columnSpacing: 14
        rowSpacing: 14
        readonly property real timeWidth: Math.max(260, Math.max(0, width - columnSpacing) * .9 / 2.35)
        Rectangle {
            id: weekCard
            objectName: "historyWeekCard"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: statsColumn.stackedPanels ? -1 : Math.max(0, activityGrid.width - activityGrid.columnSpacing - activityGrid.timeWidth)
            implicitHeight: weekPanel.implicitHeight + 32
            radius: Theme.popoverRadius
            color: Theme.mutedSurface
            border.width: 1
            border.color: Theme.subtleBorder
            ColumnLayout {
                id: weekPanel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 12
                ColumnLayout {
                    spacing: 2
                    EchoTextTagRow {
                        text: qsTr("最近 7 天")
                        font: Theme.font(15, Theme.extraBoldWeight)
                        lineHeight: 18
                        color: Theme.text
                    }
                    EchoTextTagRow {
                        Layout.topMargin: 4
                        text: qsTr("按最后播放日期统计歌曲回访")
                        font: Theme.font(12, Theme.semiboldWeight)
                        lineHeight: 18
                        color: Theme.secondaryText
                    }
                }
                RowLayout {
                    objectName: "historyWeekChart"
                    Layout.fillWidth: true
                    spacing: statsColumn.compactViewport ? 6 : 10
                    Repeater {
                        model: statsColumn.stats.recentDays || []
                        delegate: ColumnLayout {
                            id: recentDay
                            required property var modelData
                            objectName: "historyRecentDay"
                            Layout.fillWidth: true
                            spacing: 6
                            EchoTextTagRow {
                                Layout.alignment: Qt.AlignHCenter
                                text: String(recentDay.modelData.value)
                                font: Theme.font(12, Theme.extraBoldWeight)
                                lineHeight: 20
                                color: Theme.text
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 128
                                radius: Theme.controlRadius
                                color: Theme.withAlpha(Theme.text, .06)
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: Math.max(4, parent.height * Number(recentDay.modelData.ratio || 0))
                                    radius: Theme.controlRadius
                                    topLeftRadius: Theme.controlRadius
                                    topRightRadius: Theme.controlRadius
                                    bottomLeftRadius: 0
                                    bottomRightRadius: 0
                                    gradient: Gradient {
                                        GradientStop {
                                            position: 0
                                            color: Theme.accent
                                        }
                                        GradientStop {
                                            position: 1
                                            color: "#0891b2"
                                        }
                                    }
                                    Behavior on height {
                                        enabled: statsColumn.animate && Theme.panelDuration > 0
                                        NumberAnimation {
                                            duration: Theme.panelDuration
                                            easing.type: Easing.BezierSpline
                                            easing.bezierCurve: Theme.easeStandard
                                        }
                                    }
                                }
                            }
                            EchoTextTagRow {
                                Layout.alignment: Qt.AlignHCenter
                                text: recentDay.modelData.label
                                font: Theme.font(12, Theme.extraBoldWeight)
                                lineHeight: 18
                                color: Theme.text
                            }
                            EchoTextTagRow {
                                Layout.alignment: Qt.AlignHCenter
                                text: recentDay.modelData.dateLabel
                                font: Theme.font(11, Theme.boldWeight)
                                lineHeight: 16
                                color: Theme.secondaryText
                            }
                        }
                    }
                }
            }
        }
        Rectangle {
            id: timeCard
            objectName: "historyTimeCard"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: statsColumn.stackedPanels ? 0 : 260
            Layout.preferredWidth: statsColumn.stackedPanels ? -1 : activityGrid.timeWidth
            implicitHeight: timePanel.implicitHeight + 32
            radius: Theme.popoverRadius
            color: Theme.mutedSurface
            border.width: 1
            border.color: Theme.subtleBorder
            ColumnLayout {
                id: timePanel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 14
                ColumnLayout {
                    spacing: 2
                    EchoTextTagRow {
                        text: qsTr("时段偏好")
                        font: Theme.font(15, Theme.extraBoldWeight)
                        lineHeight: 18
                        color: Theme.text
                    }
                    EchoTextTagRow {
                        Layout.topMargin: 4
                        text: qsTr("按最后播放时间归类")
                        font: Theme.font(12, Theme.semiboldWeight)
                        lineHeight: 18
                        color: Theme.secondaryText
                    }
                }
                Repeater {
                    model: statsColumn.stats.timeBuckets || []
                    delegate: ColumnLayout {
                        id: timeBucket
                        required property var modelData
                        objectName: "historyTimeBucket"
                        Layout.fillWidth: true
                        spacing: 0
                        RowLayout {
                            Layout.fillWidth: true
                            EchoTextTagRow {
                                Layout.fillWidth: true
                                text: timeBucket.modelData.label
                                font: Theme.font(12, Theme.extraBoldWeight)
                                lineHeight: 16
                                color: Theme.text
                            }
                            EchoTextTagRow {
                                text: qsTr("%1 首").arg(timeBucket.modelData.value)
                                font: Theme.font(12, Theme.extraBoldWeight)
                                lineHeight: 16
                                color: Theme.text
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 7
                            Layout.preferredHeight: 7
                            clip: true
                            radius: 3.5
                            color: Theme.withAlpha(Theme.text, .07)
                            Rectangle {
                                width: parent.width * Number(timeBucket.modelData.ratio || 0)
                                height: parent.height
                                radius: 3.5
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop {
                                        position: 0
                                        color: "#059669"
                                    }
                                    GradientStop {
                                        position: 1
                                        color: Theme.accent
                                    }
                                }
                                Behavior on width {
                                    enabled: statsColumn.animate && Theme.panelDuration > 0
                                    NumberAnimation {
                                        duration: Theme.panelDuration
                                        easing.type: Easing.BezierSpline
                                        easing.bezierCurve: Theme.easeStandard
                                    }
                                }
                            }
                        }
                        EchoTextTagRow {
                            Layout.topMargin: 5
                            text: timeBucket.modelData.hint
                            font: Theme.font(11, Theme.boldWeight)
                            lineHeight: 15
                            color: Theme.secondaryText
                        }
                    }
                }
            }
        }
    }
    GridLayout {
        id: rankingGrid
        objectName: "historyRankingGrid"
        Layout.fillWidth: true
        columns: statsColumn.stackedPanels ? 1 : 2
        columnSpacing: 14
        rowSpacing: 14
        Rectangle {
            id: artistCard
            objectName: "historyArtistCard"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: statsColumn.stackedPanels ? -1 : Math.max(0, rankingGrid.width - rankingGrid.columnSpacing) * .92 / 2
            implicitHeight: artistPanel.implicitHeight + 32
            radius: Theme.popoverRadius
            color: Theme.mutedSurface
            border.width: 1
            border.color: Theme.subtleBorder
            ColumnLayout {
                id: artistPanel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    EchoTextTagRow {
                        Layout.fillWidth: true
                        text: qsTr("常听歌手")
                        font: Theme.font(15, Theme.extraBoldWeight)
                        lineHeight: 18
                        color: Theme.text
                    }
                    EchoIcon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        name: "music"
                        color: Theme.accentText
                    }
                }
                Repeater {
                    model: statsColumn.stats.topArtists || []
                    delegate: RowLayout {
                        id: artistRow
                        required property var modelData
                        required property int index
                        objectName: "historyTopArtistRow"
                        Layout.fillWidth: true
                        spacing: 10
                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            radius: Theme.controlRadius
                            color: Theme.withAlpha(Theme.text, .08)
                            EchoTextTagRow {
                                anchors.centerIn: parent
                                text: String(artistRow.index + 1)
                                font: Theme.font(12, Theme.extraBoldWeight)
                                color: Theme.text
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            EchoTextTagRow {
                                Layout.fillWidth: true
                                text: artistRow.modelData.name
                                font: Theme.font(13, Theme.extraBoldWeight)
                                lineHeight: 18
                                color: Theme.text
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.maximumWidth: 210
                                Layout.topMargin: 3
                                Layout.preferredHeight: 7
                                clip: true
                                radius: 3.5
                                color: Theme.withAlpha(Theme.text, .07)
                                Rectangle {
                                    width: parent.width * Number(artistRow.modelData.ratio || 0)
                                    height: parent.height
                                    radius: 3.5
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop {
                                            position: 0
                                            color: Theme.accent
                                        }
                                        GradientStop {
                                            position: 1
                                            color: "#d97706"
                                        }
                                    }
                                    Behavior on width {
                                        enabled: statsColumn.animate && Theme.panelDuration > 0
                                        NumberAnimation {
                                            duration: Theme.panelDuration
                                            easing.type: Easing.BezierSpline
                                            easing.bezierCurve: Theme.easeStandard
                                        }
                                    }
                                }
                            }
                            EchoTextTagRow {
                                Layout.topMargin: 3
                                text: qsTr("%1 首歌曲").arg(artistRow.modelData.songs)
                                font: Theme.font(11, Theme.boldWeight)
                                lineHeight: 15
                                color: Theme.secondaryText
                            }
                        }
                        EchoTextTagRow {
                            objectName: "historyArtistCount"
                            text: qsTr("%1 次").arg(artistRow.modelData.plays)
                            font: Theme.font(statsColumn.compactViewport ? 11 : 12, Theme.extraBoldWeight)
                            color: Theme.secondaryText
                        }
                    }
                }
            }
        }
        Rectangle {
            id: songCard
            objectName: "historyTopSongsCard"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: statsColumn.stackedPanels ? -1 : Math.max(0, rankingGrid.width - rankingGrid.columnSpacing) * 1.08 / 2
            implicitHeight: songPanel.implicitHeight + 32
            radius: Theme.popoverRadius
            color: Theme.mutedSurface
            border.width: 1
            border.color: Theme.subtleBorder
            ColumnLayout {
                id: songPanel
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    EchoTextTagRow {
                        Layout.fillWidth: true
                        text: qsTr("高频歌曲")
                        font: Theme.font(15, Theme.extraBoldWeight)
                        lineHeight: 18
                        color: Theme.text
                    }
                    EchoIcon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        name: "star"
                        color: "#d97706"
                    }
                }
                EchoTextTagRow {
                    text: qsTr("越常复听越靠前")
                    font: Theme.font(12, Theme.semiboldWeight)
                    lineHeight: 18
                    color: Theme.secondaryText
                }
                Repeater {
                    id: songRepeater
                    model: statsColumn.stats.topSongs || []
                    delegate: TopSongRow {}
                }
            }
        }
    }
    component TopSongRow: RowLayout {
        id: songRow
        readonly property int coverStatus: songCover.status
        required property var modelData
        required property int index
        objectName: "historyTopSongRow"
        Layout.fillWidth: true
        spacing: 10
        Rectangle {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            radius: Theme.controlRadius
            color: Theme.withAlpha(Theme.text, .08)
            EchoTextTagRow {
                anchors.centerIn: parent
                text: String(songRow.index + 1)
                font: Theme.font(12, Theme.extraBoldWeight)
                color: Theme.text
            }
        }
        EchoCover {
            id: songCover
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            source: songRow.modelData.cover || ""
            cornerRadius: Theme.mediaRadius
            resourceActive: statsColumn.resourceActive
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            EchoTextTagRow {
                Layout.fillWidth: true
                text: songRow.modelData.title
                font: Theme.font(13, Theme.extraBoldWeight)
                lineHeight: 18
                color: Theme.text
            }
            EchoTextTagRow {
                Layout.fillWidth: true
                Layout.topMargin: 3
                text: songRow.modelData.artist + " · " + songRow.modelData.lastPlayedLabel
                font: Theme.font(11, Theme.boldWeight)
                lineHeight: 15
                color: Theme.secondaryText
            }
        }
        EchoTextTagRow {
            objectName: "historySongCountValue"
            text: qsTr("%1 次").arg(songRow.modelData.plays)
            font: Theme.font(statsColumn.compactViewport ? 11 : 12, Theme.extraBoldWeight)
            color: Theme.secondaryText
        }
    }
    Rectangle {
        id: albumHighlight
        Layout.fillWidth: true
        implicitHeight: 64
        visible: statsColumn.stats.topAlbum !== undefined && statsColumn.stats.topAlbum.name !== undefined && String(statsColumn.stats.topAlbum.name).length > 0
        radius: Theme.popoverRadius
        color: Theme.mutedSurface
        border.width: 1
        border.color: Theme.subtleBorder
        RowLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12
            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: Theme.cardRadius
                color: Theme.withAlpha("#0891b2", .12)
                EchoIcon {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    name: "music"
                    color: "#0891b2"
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                EchoTextTagRow {
                    Layout.fillWidth: true
                    text: qsTr("最常回访专辑")
                    font: Theme.font(11, Theme.extraBoldWeight)
                    lineHeight: 15
                    color: Theme.secondaryText
                }
                EchoTextTagRow {
                    Layout.fillWidth: true
                    Layout.topMargin: 2
                    text: statsColumn.stats.topAlbum && statsColumn.stats.topAlbum.name ? String(statsColumn.stats.topAlbum.name) : ""
                    font: Theme.font(13, Theme.extraBoldWeight)
                    lineHeight: 18
                    color: Theme.text
                }
            }
            EchoTextTagRow {
                objectName: "historyAlbumCount"
                text: statsColumn.stats.topAlbum && statsColumn.stats.topAlbum.name ? qsTr("%1 次 · %2 首").arg(statsColumn.stats.topAlbum.plays).arg(statsColumn.stats.topAlbum.songs) : ""
                font: Theme.font(statsColumn.compactViewport ? 11 : 12, Theme.extraBoldWeight)
                color: Theme.secondaryText
            }
        }
    }
    EchoEnter {
        target: weekCard
        active: statsColumn.resourceActive
        duration: statsColumn.animate ? Theme.normalDuration : 0
        order: 0
    }
    EchoEnter {
        target: timeCard
        active: statsColumn.resourceActive
        duration: statsColumn.animate ? Theme.normalDuration : 0
        order: 1
    }
    EchoEnter {
        target: artistCard
        active: statsColumn.resourceActive
        duration: statsColumn.animate ? Theme.normalDuration : 0
        order: 2
    }
    EchoEnter {
        target: songCard
        active: statsColumn.resourceActive
        duration: statsColumn.animate ? Theme.normalDuration : 0
        order: 3
    }
    EchoEnter {
        target: albumHighlight
        active: statsColumn.resourceActive
        duration: statsColumn.animate ? Theme.normalDuration : 0
        order: 4
    }
}
