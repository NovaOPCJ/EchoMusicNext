pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Dialogs as NativeDialogs

// Native folder dialogs have accepted/rejected rather than Popup.closed. Delay
// disposal until the original result handler has consumed selectedFolder.
QtObject {
    id: owner
    required property Item hostItem
    required property Component sourceComponent
    property NativeDialogs.FolderDialog item: null
    readonly property var hostWindow: hostItem ? hostItem.Window.window : null
    readonly property bool available: hostItem && hostItem.visible && hostItem.enabled && hostWindow && hostWindow.visible && hostWindow.visibility !== Window.Minimized
    property int revision: 0
    function open(): void {
        if (!available || !sourceComponent)
            return;
        ++revision;
        if (!item)
            item = sourceComponent.createObject(owner, {
                parentWindow: hostItem.Window.window
            }) as NativeDialogs.FolderDialog;
        if (item)
            item.open();
    }
    function close(): void {
        if (item) {
            item.close();
            releaseAfterClose();
        }
    }
    function releaseAfterClose(): void {
        const dialog = item;
        const token = revision;
        Qt.callLater(function () {
            if (dialog && owner.item === dialog && token === owner.revision && !dialog.visible) {
                owner.item = null;
                dialog.destroy();
            }
        });
    }
    Component.onDestruction: if (item)
        item.close()
    property Connections resultEvents: Connections {
        target: owner.item
        function onVisibleChanged(): void {
            if (owner.item && !owner.item.visible)
                owner.releaseAfterClose();
        }
    }
    onAvailableChanged: if (!available)
        close()
}
