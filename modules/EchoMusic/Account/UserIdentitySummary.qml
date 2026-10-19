pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Item {
    id: summary
    property var badges: []
    property var tags: []
    property ThemeColors colors: Theme.colors
    property int horizontalAlignment: Qt.AlignLeft
    readonly property real naturalWidth: identity.naturalWidth + interests.naturalWidth + (badges.length && tags.length ? 10 : 0)
    implicitWidth: naturalWidth
    implicitHeight: groups.implicitHeight
    visible: badges.length > 0 || tags.length > 0
    // Grid owns group placement and distinct horizontal/vertical gaps.
    // Flow owns bounded tags and their natural multiline height.
    GridLayout {
        id: groups
        width: Math.min(summary.width, summary.naturalWidth)
        x: summary.horizontalAlignment === Qt.AlignRight ? summary.width - width : 0
        columns: summary.naturalWidth <= summary.width ? 2 : 1
        columnSpacing: 10
        rowSpacing: 6
        UserIdentityBadges {
            id: identity
            entries: summary.badges
            colors: summary.colors
            Layout.minimumWidth: 0
            Layout.preferredWidth: Math.min(naturalWidth, groups.width)
            Layout.alignment: summary.horizontalAlignment | Qt.AlignVCenter
        }
        Flow {
            id: interests
            visible: summary.tags.length > 0
            Layout.minimumWidth: 0
            Layout.preferredWidth: Math.min(naturalWidth, groups.width)
            Layout.alignment: summary.horizontalAlignment | Qt.AlignVCenter
            spacing: 5
            // Repeater.count can precede itemAt(); measure the actual ready delegates.
            property list<Item> measuredItems: []
            readonly property real naturalWidth: {
                let total = 0;
                for (let i = 0; i < interests.measuredItems.length; ++i) {
                    const item = interests.measuredItems[i];
                    if (item)
                        total += item.implicitWidth + (i ? spacing : 0);
                }
                return total;
            }
            Accessible.name: qsTr("用户标签")
            Repeater {
                id: items
                onItemAdded: (index, item) => interests.measuredItems = [...interests.measuredItems, item]
                onItemRemoved: (index, item) => interests.measuredItems = interests.measuredItems.filter(entry => entry !== item)
                model: summary.tags
                delegate: EchoTag {
                    required property string modelData
                    objectName: "userInterestTag"
                    width: Math.min(implicitWidth, interests.width)
                    height: implicitHeight
                    text: modelData
                    wrap: true
                    tone: EchoTag.Muted
                    colors: summary.colors
                }
            }
        }
    }
}
