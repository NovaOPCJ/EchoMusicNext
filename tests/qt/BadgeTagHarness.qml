pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme

// Contract fixture only; never a product page.
Window {
    width: 360
    height: 180
    visible: true
    color: Theme.main
    EchoBadge {
        objectName: "countBadge"
        x: 30
        y: 30
        count: 0
    }
    EchoTag {
        objectName: "statusTag"
        x: 100
        y: 30
        text: "0"
    }
    Item {
        objectName: "badgeOwner"
        x: 30
        y: 90
        width: 80
        height: 32
        EchoBadge {
            objectName: "floatingCount"
            count: "99+"
            placement: EchoBadge.Floating
        }
    }
    EchoTag {
        objectName: "contentTag"
        x: 150
        y: 90
        text: "身份"
        contentComponent: Component {
            Item {
                implicitWidth: 40
                implicitHeight: 12
            }
        }
    }
}
