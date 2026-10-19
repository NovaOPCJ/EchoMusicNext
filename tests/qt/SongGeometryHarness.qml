pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Music

Window {
    id: window
    width: 1000
    height: 360
    visible: true
    property int ordinal: 1
    property int maximumOrdinal: ordinal
    property real uiScale: 1
    SongListGeometry {
        id: songGeometry
        rowCount: window.maximumOrdinal
    }
    Item {
        width: window.width / window.uiScale
        height: window.height / window.uiScale
        scale: window.uiScale
        transformOrigin: Item.TopLeft
        SongListHeader {
            width: parent.width
            geometry: songGeometry
        }
        SongRow {
            y: 60
            width: parent.width
            index: window.ordinal - 1
            geometry: songGeometry
            title: "歌曲"
            artistText: "歌手"
            artistLinks: [
                {
                    name: "歌手",
                    id: "1"
                }
            ]
            albumId: "1"
            albumText: "专辑"
            coverUrl: ""
            durationText: "3:05"
            extraText: ""
            resourceId: "1"
            playbackAvailable: true
        }
        SongSkeletonRow {
            y: 140
            width: parent.width
            height: 60
            geometry: songGeometry
            animate: false
        }
    }
}
