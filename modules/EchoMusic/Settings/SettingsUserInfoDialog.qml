pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Settings

EchoDialog {
    id: dialog
    required property ExperimentalSettingsService service
    // Only this transient dialog owns the display snapshot; created only by the explicit diagnostic action.
    property var entries: []
    objectName: "settingsUserInfoDialog"
    heading: qsTr("用户信息")
    preferredWidth: 520
    windowHeightMargin: 100
    maximumHeight: parent ? parent.height / uiScale * .8 : 640
    flushBody: true
    onAboutToShow: entries = service.userInfo()
    onClosed: entries = []
    Connections {
        target: dialog.service
        function onIdentityChanged(): void {
            dialog.close();
        }
    }
    body: Flickable {
        id: scroll
        implicitHeight: rows.implicitHeight
        contentHeight: rows.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ColumnLayout {
            id: rows
            width: scroll.width - 24
            spacing: 10
            Repeater {
                model: dialog.entries
                delegate: Rectangle {
                    id: entry
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: content.implicitHeight + 20
                    radius: Theme.itemRadius
                    color: Theme.floating.mutedSurface
                    ColumnLayout {
                        id: content
                        anchors.fill: parent
                        anchors.margins: 10
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            text: entry.modelData.label
                            textFormat: Text.PlainText
                            font: Theme.font(11, Theme.boldWeight)
                            color: Theme.floatingSecondaryText
                        }
                        TextEdit {
                            Layout.fillWidth: true
                            text: entry.modelData.value
                            textFormat: TextEdit.PlainText
                            font: Theme.font(13, Theme.regularWeight)
                            color: Theme.floatingText
                            selectionColor: Theme.floating.accent
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            Accessible.name: entry.modelData.label
                        }
                    }
                }
            }
            Label {
                visible: dialog.entries.length === 0
                Layout.fillWidth: true
                Layout.topMargin: 20
                Layout.bottomMargin: 20
                text: qsTr("暂无用户信息，请先登录")
                horizontalAlignment: Text.AlignHCenter
                font: Theme.font(13, Theme.regularWeight)
                color: Theme.floatingSecondaryText
            }
        }
        ScrollBar.vertical: EchoPopupScrollBar {
            surface: dialog
            view: scroll
        }
    }
    footerContent: RowLayout {
        Item {
            Layout.fillWidth: true
        }
        EchoSoftButton {
            objectName: "settingsCopyAuthorization"
            text: qsTr("复制鉴权头")
            accent: true
            colors: Theme.floating
            onClicked: dialog.service.copyAuthorization()
        }
    }
}
