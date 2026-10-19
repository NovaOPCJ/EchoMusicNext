pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import EchoMusic.Theme
import EchoMusic.Controls

// Grouped tabs with a bounded set of wrapping option buttons.
EchoDialog {
    id: picker
    property var groups: []
    property string selectedId: ""
    property string activeGroup: ""
    readonly property int groupIndex: Math.max(0, groups.findIndex(group => group.label === activeGroup))
    readonly property var options: groups.length ? groups[Math.min(groupIndex, groups.length - 1)].options : []
    signal chosen(string id)
    preferredWidth: 500
    onGroupsChanged: {
        if (!groups.some(group => group.label === activeGroup))
            activeGroup = groups.length ? groups[0].label : "";
    }
    onOpened: {
        activeGroup = groups.length ? groups[0].label : "";
        for (let i = 0; i < groups.length; ++i)
            if (groups[i].options.some(option => String(option.id) === selectedId)) {
                activeGroup = groups[i].label;
                break;
            }
    }
    body: Component {
        ColumnLayout {
            width: parent.width
            spacing: 12
            EchoTabBar {
                id: groupTabs
                Layout.fillWidth: true
                visible: picker.groups.length > 1
                labels: picker.groups.map(group => String(group.label))
                currentIndex: picker.groupIndex
                onPicked: index => picker.activeGroup = picker.groups[index].label
            }
            Flickable {
                id: optionsView
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(0, Math.min(320, picker.bodyHeightLimit - (groupTabs.visible ? groupTabs.implicitHeight + 12 : 0), choices.implicitHeight + 14))
                contentHeight: choices.implicitHeight + 14
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: EchoPopupScrollBar {
                    surface: picker
                    view: optionsView
                    resourceActive: picker.visible
                }
                Flow {
                    id: choices
                    width: parent.width
                    spacing: 10
                    Repeater {
                        model: picker.options
                        delegate: EchoSoftButton {
                            required property var modelData
                            colors: Theme.floating
                            text: modelData.label
                            accent: String(modelData.id) === picker.selectedId
                            Accessible.role: Accessible.RadioButton
                            Accessible.checked: accent
                            onClicked: {
                                picker.chosen(String(modelData.id));
                                picker.close();
                            }
                        }
                    }
                }
            }
        }
    }
}
