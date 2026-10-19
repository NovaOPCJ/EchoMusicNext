pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Item {
    id: row
    objectName: "songSkeletonRow"
    required property SongListGeometry geometry
    property bool animate: true
    property bool showAlbum: true
    property bool lyricColumn: false
    // A Layout owns and recalculates its implicit height from its children.
    // Keep the 60px delegate extent on this Item so standalone ListViews and
    // parent layouts receive the same row height.
    implicitHeight: 60
    RowLayout {
        anchors.fill: parent
        spacing: 0
        Item {
            objectName: "songSkeletonIndexColumn"
            Layout.preferredWidth: row.geometry.indexWidth
            Layout.minimumWidth: row.geometry.indexWidth
            Layout.maximumWidth: row.geometry.indexWidth
            Layout.fillHeight: true
            EchoSkeleton {
                variant: EchoSkeleton.Text
                anchors.centerIn: parent
                width: 18
                height: 12
                opacity: 0.76
                animate: row.animate
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.fillHeight: false
            spacing: 12
            Item {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                EchoSurfaceShadow {
                    anchors.fill: parent
                    surface: cover
                    z: -1
                    radius: 8
                    blur: 3
                    offset: Qt.vector2d(0, 1)
                    color: Qt.rgba(0, 0, 0, Theme.dark ? .18 : .1)
                }
                EchoSkeleton {
                    id: cover
                    anchors.fill: parent
                    radius: 8
                    animate: row.animate
                }
            }
            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: 32
                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    EchoSkeleton {
                        variant: EchoSkeleton.Text
                        width: parent.width * 0.64
                        height: 13
                        animate: row.animate
                    }
                    EchoSkeleton {
                        variant: EchoSkeleton.Text
                        width: parent.width * 0.38
                        height: 11
                        animate: row.animate
                    }
                }
            }
        }
        Item {
            visible: row.showAlbum
            Layout.preferredWidth: row.lyricColumn ? row.geometry.lyricWidth : row.geometry.albumWidth
            Layout.fillHeight: true
            EchoSkeleton {
                variant: EchoSkeleton.Text
                anchors.verticalCenter: parent.verticalCenter
                width: (parent.width - 12) * 0.72
                height: 12
                animate: row.animate
            }
        }
        Item {
            Layout.preferredWidth: row.geometry.durationWidth
            Layout.fillHeight: true
            EchoSkeleton {
                variant: EchoSkeleton.Text
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 12
                animate: row.animate
            }
        }
    }
}
