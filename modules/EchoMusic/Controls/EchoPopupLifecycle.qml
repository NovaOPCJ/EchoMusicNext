pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Templates as T

QtObject {
    id: lifecycle
    required property T.Popup surface
    property EchoPopupContentClip contentClip: EchoPopupContentClip {
        surface: lifecycle.surface
    }
    // The public Template base covers Popup, Dialog, Drawer and Menu styles.
    // Connections preserves derived onAboutToShow handlers (draft preparation).
    property Connections connection: Connections {
        target: lifecycle.surface
        function onAboutToShow() {
            EchoPopupCoordinator.popupOpening(lifecycle.surface, lifecycle.surface.parent ? lifecycle.surface.parent.Overlay.overlay : null);
        }
    }
}
