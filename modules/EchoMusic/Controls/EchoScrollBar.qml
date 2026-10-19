pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme

ScrollBar {
    id: bar
    // Geometry can belong to a page while Qt's attached ScrollBar keeps the
    // size, position and input mapping connected to its virtualized Flickable.
    property Item hoverTarget: parent
    property bool resourceActive: true
    readonly property bool revealed: resourceActive && (areaHover.hovered || hovered || pressed || active || hideDelay.running)
    readonly property int effectDuration: Theme.normalDuration > 0 ? 200 : 0
    minimumSize: Math.min(1, 30 / Math.max(1, availableHeight))
    implicitWidth: 12
    leftPadding: 2
    rightPadding: 2
    topPadding: 6
    bottomPadding: 6
    hoverEnabled: true
    visible: resourceActive && size < 1
    background: null
    function revealTemporarily() {
        if (resourceActive)
            hideDelay.restart();
    }
    onPositionChanged: revealTemporarily()
    onActiveChanged: revealTemporarily()
    onHoveredChanged: {
        if (!hovered)
            revealTemporarily();
    }
    onPressedChanged: {
        if (!pressed)
            revealTemporarily();
    }
    onResourceActiveChanged: {
        if (!resourceActive)
            hideDelay.stop();
    }
    HoverHandler {
        id: areaHover
        objectName: "scrollbarAreaHover"
        parent: bar.hoverTarget
        enabled: bar.resourceActive
        onHoveredChanged: {
            if (hovered)
                hideDelay.stop();
            else
                bar.revealTemporarily();
        }
    }
    Timer {
        id: hideDelay
        interval: 600
    }
    contentItem: Item {
        opacity: bar.revealed ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: bar.effectDuration
            }
        }
        Rectangle {
            objectName: "scrollThumb"
            anchors.right: parent.right
            height: parent.height
            width: thumbHover.hovered || bar.pressed ? 8 : 4
            radius: width / 2
            color: Theme.withAlpha(Theme.text, bar.pressed || thumbHover.hovered ? 0.45 : 0.22)
            HoverHandler {
                id: thumbHover
                objectName: "scrollbarThumbHover"
                enabled: bar.resourceActive
            }
            Behavior on width {
                NumberAnimation {
                    duration: bar.effectDuration
                }
            }
            Behavior on color {
                ColorAnimation {
                    duration: bar.effectDuration
                }
            }
        }
    }
}
