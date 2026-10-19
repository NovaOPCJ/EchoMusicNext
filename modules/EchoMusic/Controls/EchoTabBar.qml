pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme

Rectangle {
    id: tabs
    required property list<string> labels
    property list<string> glyphs: []
    property list<string> counts: []
    property int countTone: EchoBadge.Muted
    // Optional knowledge flags preserve unknown counts until a category is read.
    property list<bool> countKnown: []
    property list<bool> markedTabs: []
    property string markerDescription: qsTr("当前项位于%1")
    property int currentIndex: 0
    property bool compact: false
    // Keep a 7px icon-to-label gap.
    property real contentSpacing: 7
    // Tab labels use natural letter spacing.
    property real labelLetterSpacing: 0
    property ThemeColors colors: Theme.colors
    property list<bool> availableTabs: []
    property bool radioGroup: false
    // Media surfaces can scope the shared tab colors to white.
    property color selectionColor: colors.selected
    property color selectionTextColor: colors.accentText
    property color hoverColor: colors.mutedSurface
    property string accessibleName: qsTr("选项卡")
    signal picked(int index)
    function available(index: int): bool {
        return availableTabs.length === 0 || availableTabs[index];
    }
    function boundary(last: bool): int {
        for (let i = 0; i < labels.length; ++i) {
            const candidate = last ? labels.length - 1 - i : i;
            if (available(candidate))
                return candidate;
        }
        return -1;
    }
    implicitHeight: compact ? 32 : 44
    radius: Theme.itemRadius
    color: tabs.colors.mutedSurface
    Accessible.role: radioGroup ? Accessible.Grouping : Accessible.PageTabList
    Accessible.name: accessibleName
    // Use 4px padding, or 3px for compact tabs.
    readonly property real inset: compact ? 3 : 4
    readonly property real tabWidth: (width - inset * 2 - Math.max(0, labels.length - 1) * 4) / Math.max(1, labels.length)
    Rectangle {
        objectName: "tabSelection"
        x: tabs.inset + tabs.currentIndex * (tabs.tabWidth + 4)
        y: tabs.inset
        width: tabs.tabWidth
        height: tabs.height - tabs.inset * 2
        radius: Theme.itemRadius
        color: tabs.selectionColor
        visible: tabs.currentIndex >= 0 && tabs.currentIndex < tabs.labels.length
        Behavior on x {
            NumberAnimation {
                duration: Theme.normalDuration
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeStandard
            }
        }
    }
    Repeater {
        id: buttons
        model: tabs.labels
        delegate: Button {
            id: tab
            required property int index
            required property string modelData
            objectName: "tab-" + index
            enabled: tabs.available(index)
            opacity: enabled ? 1 : 0.35
            x: tabs.inset + index * (tabs.tabWidth + 4)
            y: tabs.inset
            width: tabs.tabWidth
            height: tabs.height - tabs.inset * 2
            text: modelData
            padding: 0
            hoverEnabled: true
            // Qt forbids removing tab-focus eligibility from the focused item.
            activeFocusOnTab: activeFocus || index === (tabs.currentIndex >= 0 ? tabs.currentIndex : tabs.boundary(false))
            Accessible.role: tabs.radioGroup ? Accessible.RadioButton : Accessible.PageTab
            Accessible.name: text
            Accessible.selected: !tabs.radioGroup && index === tabs.currentIndex
            Accessible.checkable: tabs.radioGroup
            Accessible.checked: tabs.radioGroup && index === tabs.currentIndex
            contentItem: Item {
                implicitWidth: tabContent.implicitWidth
                implicitHeight: tabContent.implicitHeight
                RowLayout {
                    id: tabContent
                    anchors.centerIn: parent
                    width: Math.min(implicitWidth, parent.width)
                    spacing: tabs.contentSpacing
                    EchoIcon {
                        visible: tabs.glyphs.length > tab.index
                        name: visible ? tabs.glyphs[tab.index] : ""
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        color: tab.index === tabs.currentIndex ? tabs.selectionTextColor : tabs.colors.secondaryText
                    }
                    EchoTextTagRow {
                        objectName: "tabLabel-" + tab.index
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: tab.text
                        font: Theme.spacedFont(tabs.compact ? 12 : 13, Theme.semiboldWeight, tabs.labelLetterSpacing)
                        color: tab.index === tabs.currentIndex ? tabs.selectionTextColor : tab.hovered ? tabs.colors.text : tabs.colors.secondaryText
                        spacing: tabs.contentSpacing
                        wrapTags: false
                        EchoStatusDot {
                            objectName: "tabCurrentDot"
                            visible: tabs.markedTabs.length > tab.index && tabs.markedTabs[tab.index]
                            color: tabs.colors.accent
                            accessibleName: tabs.markerDescription.arg(tab.text)
                        }
                        EchoBadge {
                            visible: tabs.counts.length > tab.index && (tabs.countKnown.length <= tab.index || tabs.countKnown[tab.index])
                            count: visible ? tabs.counts[tab.index] : undefined
                            tone: tab.index === tabs.currentIndex ? EchoBadge.Accent : tabs.countTone
                            colors: tabs.colors
                        }
                    }
                }
            }
            background: Rectangle {
                radius: Theme.itemRadius
                color: tab.hovered && tab.index !== tabs.currentIndex ? tabs.hoverColor : "transparent"
                border.width: tab.visualFocus ? 2 : 0
                border.color: Theme.accent
            }
            function select(next: int) {
                if (next < 0 || !tabs.available(next))
                    return;
                tabs.picked(next);
                buttons.itemAt(next).forceActiveFocus();
            }
            function move(direction: int) {
                for (let i = 1; i <= tabs.labels.length; ++i) {
                    const next = (index + i * direction + tabs.labels.length) % tabs.labels.length;
                    if (tabs.available(next)) {
                        select(next);
                        return;
                    }
                }
            }
            onClicked: tabs.picked(index)
            Keys.onRightPressed: move(1)
            Keys.onLeftPressed: move(-1)
            Keys.onPressed: event => {
                if (tabs.radioGroup && (event.key === Qt.Key_Up || event.key === Qt.Key_Down)) {
                    move(event.key === Qt.Key_Up ? -1 : 1);
                    event.accepted = true;
                } else if (event.key === Qt.Key_Home) {
                    select(tabs.boundary(false));
                    event.accepted = true;
                } else if (event.key === Qt.Key_End) {
                    select(tabs.boundary(true));
                    event.accepted = true;
                }
            }
        }
    }
}
