pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Theme
import EchoMusic.Controls
import EchoMusic.Search

Item {
    id: status
    required property SearchModel service
    property real listPadding: 24
    implicitHeight: listPadding + (service.count > 0 || service.hasMore ? 50 : 0) + 40
    RowLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        y: status.listPadding + 8
        spacing: 12
        Text {
            text: status.service.state === SearchModel.Error ? status.service.errorMessage : status.service.state === SearchModel.LoadingMore ? qsTr("加载更多中...") : status.service.hasMore ? qsTr("继续下滑加载更多") : status.service.count > 0 ? qsTr("没有更多结果了") : ""
            color: Theme.secondaryText
            font: Theme.font(13, Theme.mediumWeight)
        }
        EchoSoftButton {
            implicitHeight: 30
            visible: status.service.state === SearchModel.Error && status.service.count > 0
            text: qsTr("重试加载")
            onClicked: status.service.retry()
        }
    }
}
