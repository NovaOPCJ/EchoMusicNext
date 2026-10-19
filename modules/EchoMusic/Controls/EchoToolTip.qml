pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQml.Models

// Trigger configuration only: no native Popup, Text, anchor observer or effect
// exists until this anchor actively requests its explanation. Instantiator is
// non-visual, so declarations inside layouts never add an extra layout item.
QtObject {
    id: tip
    required property Item parent
    property string text: ""
    property string description: ""
    property bool requestedVisible: false
    property int delay: 500
    property real anchorPosition: 0.5
    property bool dismissedUntilHoverEnds: false
    readonly property var hostWindow: parent ? parent.Window.window : null
    readonly property Item overlayItem: parent ? parent.Overlay.overlay : null
    readonly property bool eligible: requestedVisible && parent !== null && parent.visible && parent.enabled && hostWindow !== null && hostWindow.visible && hostWindow.visibility !== Window.Minimized && (description.length > 0 || text.length > 0)
    readonly property bool triggered: eligible && !dismissedUntilHoverEnds
    property bool delayElapsed: false
    readonly property EchoToolTipPopup popup: instance.count > 0 ? instance.objectAt(0) as EchoToolTipPopup : null
    readonly property bool visible: popup !== null && popup.visible
    onRequestedVisibleChanged: {
        if (!requestedVisible)
            dismissedUntilHoverEnds = false;
    }
    onTriggeredChanged: {
        if (!triggered)
            delayElapsed = false;
    }
    onDelayChanged: delayElapsed = false
    property Timer requestDelay: Timer {
        interval: Math.max(1, tip.delay)
        running: tip.triggered && tip.delay > 0 && !tip.delayElapsed
        onTriggered: tip.delayElapsed = true
    }
    property Instantiator instance: Instantiator {
        model: 1
        active: tip.triggered && (tip.delay <= 0 || tip.delayElapsed)
        delegate: EchoToolTipPopup {
            controller: tip
            parent: tip.parent
            text: tip.text
            description: tip.description
            anchorPosition: tip.anchorPosition
        }
    }
    property Connections popupEvents: Connections {
        target: tip.eligible ? EchoPopupCoordinator : null
        function onNativeMenuOpening() {
            tip.dismissedUntilHoverEnds = true;
        }
        function onHintOpening(surface, overlay) {
            const hint = surface as EchoToolTipPopup;
            if ((!hint || hint.controller !== tip) && overlay === tip.overlayItem)
                tip.dismissedUntilHoverEnds = true;
        }
        function onPopupOpening(surface, overlay) {
            if (overlay && overlay === tip.overlayItem)
                tip.dismissedUntilHoverEnds = true;
        }
    }
}
