pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Music

// History header with themed artwork, count and clear action.
// The page owns tab routes and content.
SongDetailHeader {
    id: header
    required property HistoryService service
    property bool showClear: true
    signal shareRequested
    signal clearRequested
    heading: qsTr("播放历史")
    typeLabel: "HISTORY"
    playable: header.service.songs.count > 0
    coverComponent: Component {
        ThemedIconCover {
            cornerRadius: Theme.mediaRadius / header.coverScale
            resourceActive: header.resourceActive
            iconPath: "M3 12a9 9 0 1 0 18 0a9 9 0 0 0-18 0M12 7v5l3 3"
        }
    }
    // Place the explanatory line before the song count.
    detailsComponent: Component {
        EchoTextTagRow {
            objectName: "historyDescription"
            text: qsTr("记录过往播放轨迹，快速回溯曾听过的歌曲与内容。")
            color: Theme.secondaryText
            font: Theme.font(13, Theme.semiboldWeight)
            fillTextWidth: true
        }
    }
    secondaryDetailsComponent: Component {
        EchoTextTagRow {
            objectName: "historySongCount"
            text: qsTr("%1 首歌曲").arg(header.service.songs.totalCount)
            color: Theme.secondaryText
            font: Theme.font(11, Theme.semiboldWeight)
            fillTextWidth: true
        }
    }
    additionalActions: Component {
        RowLayout {
            EchoSoftButton {
                objectName: "historyClearAction"
                visible: header.showClear
                text: qsTr("清空历史")
                glyph: "trash"
                enabled: header.service.songs.count > 0
                onClicked: header.clearRequested()
            }
            EchoSoftButton {
                objectName: "historyStatsShareAction"
                visible: !header.showClear
                text: qsTr("分享")
                glyph: "share"
                enabled: !header.service.statsShare.busy
                onClicked: header.shareRequested()
            }
        }
    }
    collapsedActions: Component {
        RowLayout {
            EchoIconButton {
                objectName: "historyClearCollapsed"
                visible: header.showClear
                glyph: "trash"
                text: qsTr("清空播放历史")
                enabled: header.service.songs.count > 0
                onClicked: header.clearRequested()
            }
            EchoIconButton {
                objectName: "historyStatsShareCollapsed"
                visible: !header.showClear
                glyph: "share"
                text: qsTr("分享")
                enabled: !header.service.statsShare.busy
                onClicked: header.shareRequested()
            }
        }
    }
}
