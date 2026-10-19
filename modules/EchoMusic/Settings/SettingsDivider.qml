pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import EchoMusic.Theme

Rectangle {
    Layout.fillWidth: true
    implicitHeight: 1
    color: Theme.withAlpha(Theme.floatingText, 0.12)
}
