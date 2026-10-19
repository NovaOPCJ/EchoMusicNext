pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Music

// Album header using the shared sliver geometry and album-specific metadata rows.
SongDetailHeader {
    id: header
    required property AlbumDetailContext detailContext
    property var router: null
    property string shareId: ""
    readonly property var info: detailContext.detail
    readonly property var artistList: info.artists !== undefined && info.artists !== null ? info.artists : []
    readonly property int songCount: detailContext.songs.totalCount
    readonly property string publishLine: {
        const parts = [];
        const published = String(info.published_at || "").split(" ")[0];
        if (published.length > 0)
            parts.push(published);
        parts.push(qsTr("%1 首歌曲").arg(header.songCount));
        return parts.join(" • ");
    }
    readonly property string categoryLine: [String(info.category || ""), String(info.language || "")].filter(value => value.length > 0).join(" • ")
    // Artist links require an ID and an available target route.
    // Track registry changes because plugins can register pages.
    property bool artistNavigation: false
    function refreshArtistNavigation(): void {
        header.artistNavigation = !!(header.router && header.router.available("artist-detail"));
    }
    Component.onCompleted: refreshArtistNavigation()
    Connections {
        target: header.router
        ignoreUnknownSignals: true
        function onRegistryChanged(): void {
            header.refreshArtistNavigation();
        }
    }
    additionalActions: Component {
        RowLayout {
            spacing: 8
            EchoSoftButton {
                objectName: "detailCollectButton"
                visible: header.detailContext.collectionAvailable
                text: header.detailContext.collected ? qsTr("已收藏") : qsTr("收藏")
                glyph: header.detailContext.collected ? "heart-filled" : "heart"
                accent: header.detailContext.collected
                enabled: !header.detailContext.collectionBusy
                onClicked: header.detailContext.toggleCollection()
            }

            EchoSoftButton {
                objectName: "detailShareButton"
                visible: !!header.detailContext.actions.sharing
                text: qsTr("分享")
                glyph: "share"
                onClicked: header.detailContext.actions.sharing.shareAlbum(header.shareId, String(header.info.title || ""))
            }
        }
    }
    collapsedActions: Component {
        RowLayout {
            spacing: 8
            EchoIconButton {
                objectName: "detailCollectCollapsed"
                visible: header.detailContext.collectionAvailable
                text: header.detailContext.collected ? qsTr("取消收藏") : qsTr("收藏")
                tooltipText: text
                glyph: header.detailContext.collected ? "heart-filled" : "heart"
                foreground: header.detailContext.collected ? Theme.accentText : Theme.iconMain
                enabled: !header.detailContext.collectionBusy
                onClicked: header.detailContext.toggleCollection()
            }

            EchoIconButton {
                objectName: "detailShareCollapsed"
                visible: !!header.detailContext.actions.sharing
                text: qsTr("分享")
                glyph: "share"
                onClicked: header.detailContext.actions.sharing.shareAlbum(header.shareId, String(header.info.title || ""))
            }
        }
    }
    heading: header.info.title || qsTr("专辑")
    typeLabel: "ALBUM"
    description: header.info.description || ""
    playable: header.detailContext.songs.count > 0
    loading: header.detailContext.detailLoading && !header.info.id
    coverComponent: Component {
        EchoCover {
            objectName: "albumDetailCover"
            source: header.info.cover_url || ""
            resourceActive: header.resourceActive
            cornerRadius: Theme.mediaRadius / header.coverScale
            placeholderGlyph: "vinyl"
            placeholderGlyphSize: 32
        }
    }
    detailsComponent: Component {
        RowLayout {
            objectName: "albumArtistRow"
            spacing: 4
            Repeater {
                model: header.artistList
                delegate: RowLayout {
                    id: artistDelegate
                    required property var modelData
                    required property int index
                    spacing: 4
                    Button {
                        id: artistButton
                        objectName: "albumArtistLink"
                        flat: true
                        padding: 0
                        background: null
                        enabled: header.artistNavigation && artistDelegate.modelData.id !== undefined && artistDelegate.modelData.id !== null && String(artistDelegate.modelData.id).length > 0 && String(artistDelegate.modelData.id) !== "0"
                        Accessible.name: String(artistDelegate.modelData.name || "")
                        onClicked: header.router.navigate("artist-detail", {}, String(artistDelegate.modelData.id))
                        contentItem: EchoTextTagRow {
                            text: String(artistDelegate.modelData.name || "")
                            font: Theme.font(13, Theme.semiboldWeight)
                            color: Theme.text
                        }
                        HoverHandler {
                            enabled: artistButton.enabled
                            cursorShape: Qt.PointingHandCursor
                        }
                    }
                    EchoTextTagRow {
                        visible: artistDelegate.index < header.artistList.length - 1
                        text: "/"
                        font: Theme.font(13, Theme.semiboldWeight)
                        color: Theme.secondaryText
                    }
                }
            }
            Item {
                Layout.fillWidth: true
            }
        }
    }
    secondaryDetailsComponent: Component {
        EchoTextTagRow {
            objectName: "albumPublishLine"
            text: header.publishLine
            color: Theme.secondaryText
            font: Theme.font(11, Theme.semiboldWeight)
            fillTextWidth: true
        }
    }
    tertiaryDetailsComponent: header.categoryLine.length > 0 ? categoryDetails : null
    Component {
        id: categoryDetails
        EchoTextTagRow {
            objectName: "albumCategoryLine"
            text: header.categoryLine
            color: Theme.secondaryText
            font: Theme.font(11, Theme.semiboldWeight)
            fillTextWidth: true
        }
    }
}
