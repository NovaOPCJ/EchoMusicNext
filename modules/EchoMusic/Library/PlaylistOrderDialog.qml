pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Library

EchoDialog {
    id: dialog
    objectName: "playlistOrderDialog"
    /// The shared order editor; the caller decides which target is loaded
    /// (sidebar playlist kinds, favorites tracks or owned playlist tracks).
    required property PlaylistOrderModel editor
    /// Sidebar playlist kind (0=自建 / 1=收藏) handed to editor.open().
    property int playlistTab: 0
    /// Account-owned track target: the caller supplies a local list ID and title;
    /// the model loads that list through its tracks entry.
    property bool tracksMode: false
    property real tracksListId: 0
    property string tracksTitle: ""
    readonly property bool loading: editor.busy && !editor.saving && editor.count === 0
    property int dragIndex: -1
    property int dropIndex: -1
    property real pointerY: 0
    property string dragTitle: ""
    preferredWidth: 540
    height: Math.min(480, parent ? Math.max(120, parent.height / uiScale - 96) : 480)
    maximumHeight: 480
    windowHeightMargin: 96
    heading: dialog.tracksMode ? qsTr("调整歌曲顺序") : editor.tab === 0 ? qsTr("调整自建歌单顺序") : qsTr("调整收藏歌单顺序")
    // 两种模式共用保存到酷狗的说明。
    description: qsTr("调整列表顺序并保存到酷狗")
    closable: !editor.saving
    closePolicy: editor.saving ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    function load(): void {
        if (dialog.tracksMode)
            dialog.editor.openTracks(dialog.tracksListId, dialog.tracksTitle);
        else
            dialog.editor.open(dialog.playlistTab);
    }
    onOpened: dialog.load()
    onClosed: editor.close()
    Connections {
        target: dialog.editor
        function onSaved() {
            dialog.close();
        }
        function onInvalidated() {
            dialog.close();
        }
    }
    body: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            visible: dialog.tracksMode
            text: dialog.tracksTitle
            wrapMode: Text.Wrap
            color: Theme.floatingText
            font: Theme.font(13, Theme.mediumWeight)
        }
        Label {
            Layout.fillWidth: true
            // 曲目模式仅显示首句；置顶规则说明只用于歌单模式。
            text: dialog.tracksMode ? qsTr("拖动排序，或选中后移动到指定位置。") : qsTr("拖动排序，或选中后移动到指定位置。默认收藏和我喜欢保持置顶。")
            wrapMode: Text.Wrap
            color: Theme.floatingSecondaryText
            font: Theme.font(12, Theme.regularWeight)
        }
        RowLayout {
            Layout.fillHeight: false
            spacing: 5
            visible: dialog.editor.errorMessage.length > 0
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: dialog.editor.errorMessage
                wrapMode: Text.Wrap
                color: Theme.floating.dangerText
                font: Theme.font(12, Theme.regularWeight)
            }
            EchoButton {
                text: qsTr("重新加载")
                size: EchoControlSize.Standard
                font: Theme.font(12, Theme.semiboldWeight)
                colors: Theme.floating
                accent: false
                enabled: !dialog.editor.busy
                onClicked: dialog.load()
            }
        }
        EchoSkeleton {
            Layout.fillWidth: true
            implicitHeight: 44
            visible: dialog.loading
            animate: dialog.opened && dialog.loading
            color: Theme.floating.mutedSurface
        }
        Rectangle {
            visible: !dialog.loading
            Layout.fillWidth: true
            implicitHeight: tools.implicitHeight + 12
            color: Theme.floating.mutedSurface
            radius: Theme.cardRadius
            RowLayout {
                id: tools
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6
                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: implicitWidth
                    text: dialog.tracksMode ? qsTr("%1 首歌曲").arg(dialog.editor.count) : qsTr("%1 个歌单").arg(dialog.editor.count)
                    color: Theme.floatingSecondaryText
                    font: Theme.font(12, Theme.regularWeight)
                }
                GridLayout {
                    id: toolGrid
                    Layout.preferredWidth: 269
                    Layout.maximumWidth: 269
                    Layout.minimumWidth: 176
                    Layout.fillWidth: true
                    columns: width >= 269 ? 3 : 1
                    columnSpacing: 6
                    rowSpacing: 6
                    RowLayout {
                        Layout.fillHeight: false
                        Layout.alignment: Qt.AlignRight
                        spacing: 6
                        EchoIconButton {
                            glyph: "arrow-up"
                            text: qsTr("上移")
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            padding: 8
                            enabled: !dialog.editor.busy && dialog.editor.selected > 0
                            onClicked: {
                                dialog.editor.move(dialog.editor.selected, dialog.editor.selected - 1);
                                list.positionViewAtIndex(dialog.editor.selected, ListView.Contain);
                            }
                        }
                        EchoIconButton {
                            glyph: "arrow-narrow-down"
                            text: qsTr("下移")
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            padding: 8
                            enabled: !dialog.editor.busy && dialog.editor.selected >= 0 && dialog.editor.selected + 1 < dialog.editor.count
                            onClicked: {
                                dialog.editor.move(dialog.editor.selected, dialog.editor.selected + 1);
                                list.positionViewAtIndex(dialog.editor.selected, ListView.Contain);
                            }
                        }
                    }
                    Rectangle {
                        visible: toolGrid.columns === 3
                        Layout.preferredWidth: 11
                        Layout.preferredHeight: 16
                        color: "transparent"
                        Rectangle {
                            anchors.centerIn: parent
                            width: 1
                            height: 16
                            color: Theme.floating.controlBorder
                        }
                    }
                    RowLayout {
                        Layout.fillHeight: false
                        Layout.alignment: Qt.AlignRight
                        spacing: 6
                        Label {
                            text: qsTr("移到")
                            color: Theme.floatingSecondaryText
                            font: Theme.font(12, Theme.regularWeight)
                        }
                        EchoSpinBox {
                            id: position
                            objectName: "playlistOrderPosition"
                            from: 1
                            to: Math.max(1, dialog.editor.count)
                            suffix: qsTr("位")
                            size: EchoControlSize.Small
                            suffixFont: Theme.font(11, Theme.regularWeight)
                            onAccepted: {
                                dialog.editor.move(dialog.editor.selected, value - 1);
                                list.positionViewAtIndex(dialog.editor.selected, ListView.Contain);
                            }
                            colors: Theme.floating
                            value: 1
                            Layout.preferredWidth: 92
                            Layout.preferredHeight: 32
                            enabled: !dialog.editor.busy && dialog.editor.count > 0
                            font: Theme.font(12, Theme.mediumWeight)
                            Accessible.name: qsTr("目标位置")
                        }
                        Connections {
                            target: dialog.editor
                            function onChanged() {
                                position.value = Math.max(1, dialog.editor.selected + 1);
                            }
                        }
                        EchoButton {
                            text: qsTr("移动")
                            size: EchoControlSize.Standard
                            font: Theme.font(12, Theme.semiboldWeight)
                            colors: Theme.floating
                            accent: false
                            enabled: position.enabled
                            onClicked: {
                                position.commit();
                                dialog.editor.move(dialog.editor.selected, position.value - 1);
                                list.positionViewAtIndex(dialog.editor.selected, ListView.Contain);
                            }
                        }
                    }
                }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60
            clip: true
            ListView {
                id: list
                objectName: "playlistOrderList"
                anchors.fill: parent
                visible: !dialog.loading
                model: dialog.editor
                reuseItems: true
                cacheBuffer: 52 * 6
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: EchoPopupScrollBar {
                    surface: dialog
                    view: list
                    resourceActive: dialog.opened
                }
                DragHandler {
                    id: reorder
                    target: null
                    enabled: !dialog.editor.busy && dialog.editor.count > 0
                    cursorShape: Qt.ArrowCursor
                    onActiveChanged: {
                        if (active) {
                            dialog.pointerY = centroid.position.y;
                            dialog.dragIndex = list.indexAt(centroid.pressPosition.x, centroid.pressPosition.y + list.contentY);
                            dialog.dropIndex = dialog.dragIndex;
                            dialog.dragTitle = dialog.editor.titleAt(dialog.dragIndex);
                            dialog.editor.select(dialog.dragIndex);
                        } else {
                            dialog.editor.move(dialog.dragIndex, dialog.dropIndex);
                            dialog.dragIndex = -1;
                        }
                    }
                    onTranslationChanged: if (active) {
                        dialog.pointerY = centroid.position.y;
                        dialog.dropIndex = Math.max(0, Math.min(dialog.editor.count - 1, Math.floor((list.contentY + dialog.pointerY) / 52)));
                    }
                }
                Timer {
                    interval: 16
                    repeat: true
                    running: dialog.dragIndex >= 0 && dialog.opened
                    onTriggered: {
                        const edge = 48;
                        const delta = dialog.pointerY < edge ? -Math.min(12, (edge - dialog.pointerY) / 4) : dialog.pointerY > list.height - edge ? Math.min(12, (dialog.pointerY - list.height + edge) / 4) : 0;
                        list.contentY = Math.max(0, Math.min(Math.max(0, list.contentHeight - list.height), list.contentY + delta));
                        dialog.dropIndex = Math.max(0, Math.min(dialog.editor.count - 1, Math.floor((list.contentY + dialog.pointerY) / 52)));
                    }
                }
                delegate: ItemDelegate {
                    id: row
                    objectName: "playlistOrderRow"
                    required property int index
                    required property string title
                    required property string subtitle
                    width: list.width
                    height: 52
                    padding: 14
                    enabled: !dialog.editor.busy
                    onClicked: dialog.editor.select(index)
                    Accessible.name: title
                    background: Rectangle {
                        color: dialog.editor.selected === row.index ? Theme.withAlpha(Theme.floating.accent, 0.08) : row.hovered ? Theme.floating.hover : "transparent"
                        Rectangle {
                            width: 2
                            height: parent.height
                            visible: dialog.editor.selected === row.index
                            color: Theme.floating.accent
                        }
                    }
                    contentItem: RowLayout {
                        spacing: 10
                        Grid {
                            Layout.preferredWidth: 10
                            Layout.preferredHeight: 14
                            columns: 2
                            spacing: 3
                            Repeater {
                                model: 6
                                delegate: Rectangle {
                                    width: 2
                                    height: 2
                                    radius: 1
                                    color: Theme.floatingSecondaryText
                                }
                            }
                        }

                        Label {
                            text: row.index + 1
                            Layout.preferredWidth: 34
                            horizontalAlignment: Text.AlignRight
                            color: Theme.floatingSecondaryText
                            font: Theme.font(12, Theme.regularWeight)
                        }
                        ColumnLayout {
                            Layout.fillHeight: false
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: row.title
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                color: Theme.floatingText
                                font: Theme.font(13, Theme.mediumWeight)
                            }
                            Label {
                                text: row.subtitle
                                color: Theme.floatingSecondaryText
                                font: Theme.font(11, Theme.regularWeight)
                            }
                        }
                    }
                    opacity: dialog.dragIndex === index ? 0 : 1
                    transform: Translate {
                        y: dialog.dragIndex < 0 ? 0 : dialog.dropIndex > dialog.dragIndex && row.index > dialog.dragIndex && row.index <= dialog.dropIndex ? -52 : dialog.dropIndex < dialog.dragIndex && row.index < dialog.dragIndex && row.index >= dialog.dropIndex ? 52 : 0
                        Behavior on y {
                            NumberAnimation {
                                duration: 160
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }
            }
            Column {
                anchors.fill: parent
                visible: dialog.loading
                Repeater {
                    model: 5
                    delegate: RowLayout {
                        id: loadingRow
                        required property int index
                        width: list.width
                        height: 52
                        spacing: 16
                        EchoSkeleton {
                            Layout.leftMargin: 14
                            implicitWidth: 24
                            implicitHeight: 12
                            animate: dialog.opened && dialog.loading
                            color: Theme.floating.mutedSurface
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 8
                            EchoSkeleton {
                                width: parent.width * (loadingRow.index % 2 ? 0.76 : 0.58)
                                height: 12
                                animate: dialog.opened && dialog.loading
                                color: Theme.floating.mutedSurface
                            }
                            EchoSkeleton {
                                width: parent.width * 0.32
                                height: 9
                                animate: dialog.opened && dialog.loading
                                color: Theme.floating.mutedSurface
                            }
                        }
                    }
                }
            }
            Rectangle {
                width: list.width
                height: 52
                y: Math.max(0, Math.min(list.height - height, dialog.pointerY - height / 2))
                visible: dialog.dragIndex >= 0
                color: Theme.mix(Theme.floating.card, Theme.floating.accent, 0.08)
                radius: Theme.cardRadius
                border.color: Theme.withAlpha(Theme.floating.accent, 0.3)
                Label {
                    anchors.fill: parent
                    anchors.margins: 14
                    text: dialog.dragTitle
                    font: Theme.font(13, Theme.mediumWeight)
                    color: Theme.floatingText
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
            }
            Label {
                anchors.centerIn: parent
                visible: dialog.editor.count === 0 && !dialog.loading
                text: dialog.editor.busy ? qsTr("正在加载并排序…") : qsTr("没有可调整顺序的条目")
                color: Theme.floatingSecondaryText
                font: Theme.font(12, Theme.regularWeight)
            }
        }
    }
    footerContent: RowLayout {
        spacing: 8
        EchoButton {
            text: qsTr("还原顺序")
            size: EchoControlSize.Standard
            font: Theme.font(12, Theme.semiboldWeight)
            colors: Theme.floating
            accent: false
            enabled: !dialog.editor.busy && dialog.editor.dirty
            onClicked: dialog.editor.restore()
        }
        Item {
            Layout.fillWidth: true
        }
        // 两种模式共用未保存修改提示。
        Label {
            visible: dialog.editor.dirty
            text: qsTr("有未保存的调整")
            color: Theme.floatingSecondaryText
            font: Theme.font(12, Theme.regularWeight)
        }
        EchoButton {
            text: qsTr("取消")
            size: EchoControlSize.Standard
            font: Theme.font(12, Theme.semiboldWeight)
            colors: Theme.floating
            accent: false
            enabled: !dialog.editor.saving
            onClicked: dialog.close()
        }
        EchoButton {
            text: dialog.editor.saving ? qsTr("正在保存…") : qsTr("保存顺序")
            size: EchoControlSize.Standard
            font: Theme.font(12, Theme.semiboldWeight)
            colors: Theme.floating
            enabled: !dialog.editor.busy && dialog.editor.dirty
            onClicked: dialog.editor.save()
        }
    }
}
