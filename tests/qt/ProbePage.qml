import QtQuick
import EchoMusic.Shell

RoutePage {
    id: page
    objectName: "probe-" + context.instanceId
    property int visualScroll: 0
    property int ticks: 0
    Timer {
        interval: 5
        repeat: true
        running: page.pageActive
        onTriggered: page.ticks++
    }
}
