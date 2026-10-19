pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Settings
import EchoMusic.Controls

ApplicationWindow {
    id: root
    required property AboutService about
    width: 900
    height: 760
    visible: true
    function showUpdate(): void {
        owner.open();
    }
    function closeUpdate(): void {
        owner.close();
    }
    readonly property QtObject popupOwner: owner
    EchoPopupLoader {
        id: owner
        hostItem: root.contentItem
        sourceComponent: Component {
            SettingsUpdateDialog {
                about: root.about
                objectName: "updateDialog"
            }
        }
    }
}
