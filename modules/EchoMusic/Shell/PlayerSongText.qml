pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Playback
import EchoMusic.Theme

// The player bar uses a bounded two-cycle marquee.
// The playback page elides text and supports copying the whole row.
Item {
    id: song
    required property PlaybackService playback
    property ThemeColors colors: Theme.colors
    property bool lyricStyle: false
    signal artistRequested(string id)
    signal albumRequested(string id)
    signal copyRequested
    clip: true
    readonly property real travel: Math.min(180, Math.max(0, textRow.implicitWidth - width))
    property real offset: 0
    readonly property bool hasTrack: playback !== null && playback.hasTrack
    readonly property string songTitle: playback ? playback.title : qsTr("未在播放")
    readonly property string artist: playback ? playback.artist : ""
    readonly property string identity: songTitle + "\n" + artist
    readonly property bool motionEnabled: Theme.store.motionEnabled
    // EchoTextTagRow's default children are badges. Keep input handlers in
    // explicit properties so its tag layout cannot adopt their hit region.
    property HoverHandler albumHover: HoverHandler {
        id: titleHover
        parent: title
        cursorShape: titleTap.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
    property TapHandler albumTap: TapHandler {
        id: titleTap
        parent: title
        enabled: !song.lyricStyle && song.playback && song.playback.albumId.length > 0 && song.playback.album.trim().length > 0
        onTapped: song.albumRequested(song.playback.albumId)
    }
    function restartMarquee(): void {
        marquee.stop();
        offset = 0;
        if (visible && !lyricStyle && Theme.store.motionEnabled && travel > 8)
            marquee.start();
    }
    onWidthChanged: restartTimer.restart()
    onVisibleChanged: restartTimer.restart()
    onLyricStyleChanged: restartTimer.restart()
    onIdentityChanged: restartTimer.restart()
    onMotionEnabledChanged: restartTimer.restart()
    HoverHandler {
        id: hover
        onHoveredChanged: if (hovered && !marquee.running)
            song.restartMarquee()
    }
    TapHandler {
        enabled: song.lyricStyle
        onTapped: song.copyRequested()
    }
    EchoToolTip {
        parent: song
        requestedVisible: hover.hovered && song.hasTrack && (song.lyricStyle ? title.truncated || artistLine.truncated : textRow.implicitWidth > song.width)
        text: song.identity
    }
    Row {
        id: textRow
        x: -song.offset
        height: song.height
        spacing: 4
        EchoTextTagRow {
            id: title
            objectName: "playerbarSongTitle"
            width: song.lyricStyle ? Math.max(0, Math.min(implicitWidth, song.width - (song.hasTrack ? 16 : 0))) : implicitWidth
            lineHeight: song.height
            text: song.songTitle
            font: Theme.font(14, Theme.boldWeight)
            color: titleHover.hovered && titleTap.enabled ? song.colors.accentText : song.colors.text
        }
        EchoTextTagRow {
            id: separator
            visible: song.hasTrack
            lineHeight: song.height
            text: "-"
            font: Theme.font(14, Theme.regularWeight)
            color: song.colors.secondaryText
        }
        EchoArtistLinks {
            id: artistLine
            objectName: "playerbarArtistLinks"
            visible: song.hasTrack
            width: song.lyricStyle ? Math.max(0, Math.min(implicitWidth, song.width * 0.52, song.width - title.width - separator.width - 8)) : implicitWidth
            lineHeight: song.height
            artists: song.playback ? song.playback.artistLinks : []
            separatorSpacing: 2
            linksEnabled: !song.lyricStyle
            onArtistRequested: id => song.artistRequested(id)
            font: Theme.font(13, Theme.regularWeight)
            color: song.colors.secondaryText
        }
    }
    Timer {
        id: restartTimer
        interval: 0
        onTriggered: song.restartMarquee()
    }
    SequentialAnimation {
        id: marquee
        loops: 2
        PropertyAction {
            target: song
            property: "offset"
            value: 0
        }
        PauseAnimation {
            duration: 1200
        }
        NumberAnimation {
            target: song
            property: "offset"
            from: 0
            to: song.travel
            duration: 7600
            easing.type: Easing.Linear
        }
        PauseAnimation {
            duration: 1200
        }
        onStopped: song.offset = 0
    }
    Component.onCompleted: restartTimer.restart()
}
