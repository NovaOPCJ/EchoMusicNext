pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Theme
import EchoMusic.Controls
import EchoMusic.Music

Item {
    id: skeleton
    property bool songs: true
    property bool video: false
    property bool pageActive: true
    property bool lyricColumn: false
    property bool showAlbum: true
    required property SongListGeometry geometry
    implicitHeight: songs ? songContent.implicitHeight + 48 : cards.implicitHeight + 52
    ColumnLayout {
        id: songContent
        anchors.left: parent.left
        anchors.right: parent.right
        visible: skeleton.songs
        spacing: 0
        RowLayout {
            Layout.fillHeight: false
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.bottomMargin: 8
            spacing: 10
            EchoSkeleton {
                variant: EchoSkeleton.Circle
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                animate: skeleton.pageActive
            }
            EchoSkeleton {
                variant: EchoSkeleton.Text
                Layout.preferredWidth: 92
                Layout.preferredHeight: 15
                animate: skeleton.pageActive
            }
            Item {
                Layout.fillWidth: true
            }
            EchoSkeleton {
                variant: EchoSkeleton.Text
                Layout.preferredWidth: 88
                Layout.preferredHeight: 36
                animate: skeleton.pageActive
            }
            EchoSkeleton {
                variant: EchoSkeleton.Text
                Layout.preferredWidth: 76
                Layout.preferredHeight: 36
                animate: skeleton.pageActive
            }
        }
        RowLayout {
            Layout.fillHeight: false
            spacing: 12
            Layout.fillWidth: true
            // 36px field + 8px top/bottom padding; border follows below.
            Layout.preferredHeight: 52
            EchoSkeleton {
                variant: EchoSkeleton.Text
                Layout.preferredWidth: 96
                Layout.preferredHeight: 13
                animate: skeleton.pageActive
            }
            Item {
                Layout.fillWidth: true
            }
            EchoSkeleton {
                variant: EchoSkeleton.Text
                Layout.preferredWidth: 164
                Layout.preferredHeight: 36
                animate: skeleton.pageActive
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.subtleBorder
        }
        // Bounded decorative rows; the live catalog keeps its typed model.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Layout.bottomMargin: 16
            spacing: 0
            Repeater {
                model: 10
                delegate: SongSkeletonRow {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 60
                    geometry: skeleton.geometry
                    animate: skeleton.pageActive
                    showAlbum: skeleton.showAlbum
                    lyricColumn: skeleton.lyricColumn
                }
            }
        }
    }
    GridLayout {
        id: cards
        y: 4
        anchors.left: parent.left
        anchors.right: parent.right
        visible: !skeleton.songs
        columns: Math.max(1, Math.floor((width + 20) / 200))
        columnSpacing: 20
        rowSpacing: 20
        Repeater {
            model: 8
            delegate: ContentSkeleton {
                Layout.fillWidth: true
                Layout.preferredWidth: (cards.width - (cards.columns - 1) * 20) / cards.columns
                Layout.preferredHeight: implicitHeight
                video: skeleton.video
                resourceActive: skeleton.pageActive
            }
        }
    }
}
