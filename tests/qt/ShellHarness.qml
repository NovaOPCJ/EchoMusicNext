import QtQuick
import EchoMusic.Shell

Item {
    required property RouteController router
    required property ShellSettings settings
    width: 800
    height: 600
    RouteHost {
        anchors.fill: parent
        router: parent.router
        settings: parent.settings
    }
}
