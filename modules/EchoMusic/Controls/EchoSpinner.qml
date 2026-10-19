pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Theme

EchoIcon {
    id: spinner
    implicitWidth: 28
    implicitHeight: 28
    name: "verification-spinner"
    color: Theme.accent
    property int rotationDuration: 800
    RotationAnimator on rotation {
        from: 0
        to: 360
        duration: spinner.rotationDuration
        loops: Animation.Infinite
        running: spinner.visible && spinner.resourceActive && Theme.store.motionEnabled
    }
}
