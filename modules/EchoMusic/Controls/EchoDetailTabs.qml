pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme

// Detail-page underline tabs. Pages own routes and content; this control
// renders labels, badges and native interaction. The 36px tab box is centered
// in a 56px row; the 2px underline sits at the tab-box bottom.
Item {
    id: root
    property list<string> labels: []
    // Numeric counts include zero. An empty string hides the badge.
    property list<var> counts: []
    property list<string> objectNames: []
    property int currentIndex: 0
    property string accessibleName: qsTr("详情选项卡")
    // Pages may override typography; defaults are 15px/700.
    property real labelPixelSize: 15
    property int labelWeight: Theme.boldWeight
    implicitHeight: 56
    implicitWidth: tabRow.implicitWidth
    signal picked(int index)
    Accessible.role: Accessible.PageTabList
    Accessible.name: root.accessibleName
    readonly property real tabBoxHeight: 36
    readonly property real tabBoxTop: (root.height - root.tabBoxHeight) / 2
    // Repeater.count can change before itemAt() returns the new delegate.
    // Track the actual delegate lifecycle so a route's initially selected tab
    // has an indicator without requiring a later user selection.
    property Item activeTabItem: null
    onCurrentIndexChanged: activeTabItem = tabs ? tabs.itemAt(currentIndex) : null
    function countVisible(index: int): bool {
        if (index < 0 || index >= root.counts.length)
            return false;
        const value = root.counts[index];
        return value !== undefined && value !== null && value !== "";
    }
    Rectangle {
        objectName: "detailTabIndicator"
        y: root.tabBoxTop + root.tabBoxHeight - 2
        height: 2
        radius: 1
        color: Theme.accent
        x: root.activeTabItem ? root.activeTabItem.x : 0
        width: root.activeTabItem ? root.activeTabItem.width : 0
        Behavior on x {
            enabled: Theme.normalDuration > 0
            NumberAnimation {
                duration: Theme.normalDuration
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeStandard
            }
        }
        Behavior on width {
            enabled: Theme.normalDuration > 0
            NumberAnimation {
                duration: Theme.normalDuration
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeStandard
            }
        }
    }
    RowLayout {
        id: tabRow
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: implicitWidth
        spacing: 32
        Repeater {
            id: tabs
            model: root.labels
            onItemAdded: (index, item) => {
                if (index === root.currentIndex)
                    root.activeTabItem = item;
            }
            onItemRemoved: (index, item) => {
                if (root.activeTabItem === item)
                    root.activeTabItem = null;
            }
            delegate: Button {
                id: tab
                required property int index
                required property string modelData
                objectName: root.objectNames.length > tab.index ? root.objectNames[tab.index] : "detailTab-" + tab.index
                flat: true
                padding: 0
                hoverEnabled: true
                activeFocusOnTab: activeFocus || tab.index === root.currentIndex
                background: null
                implicitHeight: root.tabBoxHeight
                Layout.preferredHeight: root.tabBoxHeight
                Layout.fillHeight: false
                Layout.alignment: Qt.AlignVCenter
                Accessible.role: Accessible.PageTab
                Accessible.name: tab.modelData
                Accessible.selected: tab.index === root.currentIndex
                onClicked: root.picked(tab.index)
                function select(next: int): void {
                    const item = tabs.itemAt(next);
                    if (!item)
                        return;
                    root.picked(next);
                    item.forceActiveFocus();
                }
                Keys.onRightPressed: select((tab.index + 1) % root.labels.length)
                Keys.onLeftPressed: select((tab.index + root.labels.length - 1) % root.labels.length)
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Home) {
                        select(0);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_End) {
                        select(root.labels.length - 1);
                        event.accepted = true;
                    }
                }
                contentItem: Item {
                    implicitWidth: label.implicitWidth
                    implicitHeight: root.tabBoxHeight
                    EchoTextTagRow {
                        id: label
                        objectName: "detailTabLabel-" + tab.index
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 4
                        text: tab.modelData
                        // Active and hovered labels use the main text color; the underline carries the accent.
                        font: Theme.font(root.labelPixelSize, root.labelWeight)
                        color: tab.index === root.currentIndex || tab.hovered ? Theme.text : Theme.secondaryText
                        // Keep a 4px label-to-badge gap.
                        spacing: 4
                        wrapTags: false
                        Behavior on color {
                            enabled: Theme.fastDuration > 0
                            ColorAnimation {
                                duration: Theme.fastDuration
                                easing.type: Easing.BezierSpline
                                easing.bezierCurve: Theme.easeStandard
                            }
                        }
                        EchoBadge {
                            objectName: "detailTabBadge-" + tab.index
                            visible: root.countVisible(tab.index)
                            count: visible ? root.counts[tab.index] : undefined
                            // Song-tab badges use the neutral tone.
                            tone: EchoBadge.Neutral
                        }
                    }
                }
            }
        }
    }
}
