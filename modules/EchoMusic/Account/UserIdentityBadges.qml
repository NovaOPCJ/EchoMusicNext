pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Controls
import EchoMusic.Theme

Flow {
    id: badges

    property var entries: []
    property ThemeColors colors: Theme.colors
    // Repeater.count can precede itemAt(); measure the actual ready delegates.
    property list<Item> measuredItems: []
    readonly property real naturalWidth: {
        let total = 0;
        for (let i = 0; i < badges.measuredItems.length; ++i) {
            const item = badges.measuredItems[i];
            if (item)
                total += item.implicitWidth + (i ? spacing : 0);
        }
        return total;
    }
    spacing: 5
    visible: entries.length > 0
    Accessible.name: qsTr("用户身份")
    Repeater {
        id: items
        onItemAdded: (index, item) => badges.measuredItems = [...badges.measuredItems, item]
        onItemRemoved: (index, item) => badges.measuredItems = badges.measuredItems.filter(entry => entry !== item)
        model: badges.entries
        delegate: EchoTag {
            required property var modelData
            objectName: "userIdentityBadge"
            width: Math.min(implicitWidth, badges.width)
            height: implicitHeight
            text: modelData.label
            wrap: true
            tone: EchoTag.Custom
            customTone: modelData.kind === "talent" ? "#8b5cf6" : modelData.kind === "student" ? "#0ea5e9" : modelData.kind === "membership" ? "#e8a317" : "#07c160"
            colors: badges.colors
        }
    }
}
