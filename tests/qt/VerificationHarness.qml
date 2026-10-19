pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Account
import EchoMusic.Theme

Rectangle {
    id: root
    required property VerificationService service
    width: 1100
    height: 780
    color: Theme.main
    VerificationOverlay {
        anchors.fill: parent
        service: root.service
    }
}
