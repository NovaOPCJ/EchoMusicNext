pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Item {
    id: commentState
    enum Geometry {
        LoadingHeight = 468
    }
    property bool loading: false
    property bool resourceActive: true
    implicitHeight: loading ? CommentListState.LoadingHeight : 96
    Loader {
        anchors.fill: parent
        active: commentState.loading
        sourceComponent: Column {
            Repeater {
                model: 3
                delegate: Item {
                    width: parent.width
                    height: 156
                    Rectangle {
                        width: parent.width
                        height: 144
                        radius: Theme.itemRadius
                        color: Theme.withAlpha(Theme.colors.tone, .05)
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 20
                            spacing: 12
                            EchoSkeleton {
                                variant: EchoSkeleton.Circle
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                Layout.alignment: Qt.AlignTop
                                Layout.fillHeight: false
                                animate: commentState.resourceActive
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: false
                                spacing: 0
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: false
                                    spacing: 12
                                    Item {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: false
                                        implicitWidth: 112
                                        implicitHeight: 30
                                        Column {
                                            spacing: 7
                                            EchoSkeleton {
                                                variant: EchoSkeleton.Text
                                                width: 112
                                                height: 13
                                                animate: commentState.resourceActive
                                            }
                                            EchoSkeleton {
                                                variant: EchoSkeleton.Text
                                                width: 72
                                                height: 10
                                                animate: commentState.resourceActive
                                            }
                                        }
                                    }
                                    EchoSkeleton {
                                        objectName: "commentSkeletonLike"
                                        variant: EchoSkeleton.Text
                                        Layout.preferredWidth: 54
                                        Layout.preferredHeight: 24
                                        Layout.alignment: Qt.AlignTop
                                        animate: commentState.resourceActive
                                    }
                                }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.topMargin: 14
                                    Layout.bottomMargin: 14
                                    implicitHeight: 34
                                    Column {
                                        width: parent.width
                                        spacing: 8
                                        EchoSkeleton {
                                            variant: EchoSkeleton.Text
                                            width: parent.width * .92
                                            height: 13
                                            animate: commentState.resourceActive
                                        }
                                        EchoSkeleton {
                                            variant: EchoSkeleton.Text
                                            width: parent.width * .74
                                            height: 13
                                            animate: commentState.resourceActive
                                        }
                                    }
                                }
                                EchoSkeleton {
                                    variant: EchoSkeleton.Text
                                    Layout.preferredWidth: 86
                                    Layout.preferredHeight: 12
                                    animate: commentState.resourceActive
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    Label {
        objectName: "commentEmpty"
        anchors.fill: parent
        visible: !commentState.loading
        text: qsTr("暂无评论")
        color: Theme.secondaryText
        font: Theme.font(13, Theme.semiboldWeight)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
