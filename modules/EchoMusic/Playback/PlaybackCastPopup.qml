pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Playback

// Casting panel with a visibility-scoped discovery lease.
// Device and session state remain in services; rows share the settings model.
EchoAnchoredPopup {
    id: popup
    objectName: "playbackCastPopup"
    required property PlaybackService playback
    readonly property CastService output: playback.cast
    property string pinTarget: ""
    property bool connectingHere: false
    signal settingsRequested
    preferredWidth: 304
    maximumHeight: 560
    preferAbove: true
    alignEnd: false
    showArrow: false
    padding: 9
    implicitHeight: body.implicitHeight + topPadding + bottomPadding
    onVisibleChanged: output.setBrowsingFor(popup, visible)
    onClosed: {
        pinTarget = "";
        connectingHere = false;
    }
    Component.onDestruction: output.setBrowsingFor(popup, false)
    Connections {
        target: popup.output
        function onChanged() {
            if (popup.visible && popup.connectingHere && !popup.output.busy) {
                popup.connectingHere = false;
                if (popup.output.remote && !popup.output.errorMessage.length)
                    popup.close();
            }
        }
    }
    function choose(id, needsPin) {
        if (needsPin) {
            pinTarget = id;
            return;
        }
        pinTarget = "";
        output.connectTarget(id);
        connectingHere = output.busy;
    }
    contentItem: Flickable {
        id: scroll
        clip: true
        contentWidth: width
        contentHeight: body.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: EchoPopupScrollBar {
            surface: popup
            view: scroll
            resourceActive: popup.visible
        }
        ColumnLayout {
            id: body
            width: scroll.width
            spacing: 7
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: false
                Layout.leftMargin: 2
                Layout.rightMargin: 2
                Layout.topMargin: 2
                Layout.bottomMargin: 4
                spacing: 9
                Rectangle {
                    implicitWidth: 28
                    implicitHeight: 28
                    color: Theme.withAlpha(Theme.floating.accent, 0.1)
                    radius: Theme.popoverRadius
                    EchoIcon {
                        anchors.centerIn: parent
                        width: 17
                        height: 17
                        name: "cast"
                        color: Theme.floating.accentText
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 0
                    EchoTextTagRow {
                        text: qsTr("投放")
                        font: Theme.font(13, 800)
                        color: Theme.floatingText
                    }
                    Label {
                        Layout.fillWidth: true
                        text: popup.output.activeTitle
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font: Theme.font(11, Theme.semiboldWeight)
                        color: Theme.floatingSecondaryText
                    }
                }
                RowLayout {
                    Layout.fillHeight: false
                    spacing: 5
                    EchoIconButton {
                        implicitWidth: 24
                        implicitHeight: 24
                        padding: 4.5
                        colors: Theme.floating
                        glyph: "refresh"
                        text: qsTr("刷新设备")
                        tooltipText: text
                        enabled: popup.output.enabled && !popup.output.busy && !popup.output.searching
                        onClicked: popup.output.refresh()
                    }
                    EchoIconButton {
                        objectName: "playbackCastSettings"
                        implicitWidth: 24
                        implicitHeight: 24
                        padding: 4.5
                        colors: Theme.floating
                        glyph: "settings"
                        text: qsTr("投放设置")
                        tooltipText: text
                        onClicked: {
                            popup.close();
                            popup.settingsRequested();
                        }
                    }
                    EchoSwitch {
                        objectName: "playbackCastEnabled"
                        colors: Theme.floating
                        checked: popup.output.enabled
                        Accessible.name: qsTr("启用投放")
                        onToggled: popup.output.enabled = checked
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 30
                visible: popup.output.errorMessage.length > 0 || popup.output.searching && popup.output.rowCount() === 0
                color: popup.output.errorMessage.length ? Theme.withAlpha(Theme.floating.danger, 0.1) : Theme.floating.mutedSurface
                radius: Theme.popoverRadius
                Label {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 2
                    verticalAlignment: Text.AlignVCenter
                    text: popup.output.errorMessage.length ? popup.output.errorMessage : qsTr("正在搜索投放设备...")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    font: Theme.font(11, Theme.semiboldWeight)
                    color: popup.output.errorMessage.length ? Theme.floating.dangerText : Theme.floatingSecondaryText
                }
            }
            Rectangle {
                Layout.fillWidth: true
                visible: popup.output.remote
                implicitHeight: 44
                radius: Theme.controlRadius
                color: Theme.withAlpha(Theme.floating.accent, 0.08)
                border.color: Theme.withAlpha(Theme.floating.accent, 0.24)
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 7
                    spacing: 10
                    EchoTextTagRow {
                        Layout.fillWidth: true
                        text: popup.output.remoteMedia ? qsTr("DLNA 投放") : qsTr("AirPlay 投放")
                        font: Theme.font(12, Theme.boldWeight)
                        color: Theme.floatingText
                    }
                    EchoSoftButton {
                        Layout.preferredWidth: 86
                        text: popup.output.busy ? qsTr("停止中") : qsTr("停止投放")
                        size: EchoControlSize.Compact
                        colors: Theme.floating
                        enabled: !popup.output.busy
                        onClicked: popup.output.useLocal()
                    }
                }
            }
            CastDeviceGroup {
                Layout.fillWidth: true
                output: popup.output
                heading: "DLNA"
                compact: true
                popupSurface: popup
                deviceModel: popup.output.dlna
                maximumListHeight: Math.max(28, Math.min(172, popup.contentHeightLimit - 160))
                opacity: popup.output.enabled ? 1 : 0.58
                onChoose: (id, needsPin) => popup.choose(id, needsPin)
            }
            CastDeviceGroup {
                Layout.fillWidth: true
                output: popup.output
                heading: "AirPlay"
                compact: true
                popupSurface: popup
                deviceModel: popup.output.airplay
                pinTarget: popup.pinTarget
                maximumListHeight: Math.max(28, Math.min(172, popup.contentHeightLimit - 160))
                opacity: popup.output.enabled ? 1 : 0.58
                onChoose: (id, needsPin) => popup.choose(id, needsPin)
                onSubmitPin: value => {
                    popup.output.connectTarget(popup.pinTarget, value);
                    popup.connectingHere = popup.output.busy;
                }
            }
        }
    }
}
