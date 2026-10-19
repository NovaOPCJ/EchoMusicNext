pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtQuick.Templates as T

// A background radius does not clip a Control's contentItem. Use Qt's standard
// layer mask only when content reaches the rounded frame or its inner border.
// Padded Dialog/Menu/Select content already inside the frame needs no layer.
QtObject {
    id: clipping
    objectName: "popupContentClip"
    required property T.Popup surface
    // Popup's background/contentItem are deferred. Reading them while the
    // derived recipe is still constructing can instantiate the style default
    // before its replacement. Wait for show, after the full recipe is ready.
    readonly property Item content: surface.visible ? surface.contentItem : null
    readonly property EchoFloatingSurface frame: surface.visible ? surface.background as EchoFloatingSurface : null
    readonly property real inset: frame ? frame.border.width : 0
    readonly property rect frameRect: frame && content ? Qt.rect(frame.x - content.x + inset, frame.y - content.y + inset, Math.max(0, frame.width - inset * 2), Math.max(0, frame.height - inset * 2)) : Qt.rect(0, 0, 0, 0)
    readonly property real radius: frame ? Math.min(Math.max(0, frame.radius - inset), frameRect.width / 2, frameRect.height / 2) : 0
    readonly property rect paintedRect: Qt.rect(0, 0, content ? content.width : 0, content ? content.height : 0)
    function contains(x: real, y: real): bool {
        const r = frameRect;
        if (x < r.x || y < r.y || x > r.x + r.width || y > r.y + r.height)
            return false;
        const cx = Math.max(r.x + radius, Math.min(x, r.x + r.width - radius));
        const cy = Math.max(r.y + radius, Math.min(y, r.y + r.height - radius));
        return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius;
    }
    readonly property bool needed: frame !== null && content !== null && paintedRect.width > 0 && paintedRect.height > 0 && (!contains(paintedRect.x, paintedRect.y) || !contains(paintedRect.x + paintedRect.width, paintedRect.y) || !contains(paintedRect.x, paintedRect.y + paintedRect.height) || !contains(paintedRect.x + paintedRect.width, paintedRect.y + paintedRect.height))
    readonly property bool active: needed && frame.resourceActive
    readonly property QtObject contentLayer: content ? content.layer : null
    property Binding layerEnabled: Binding {
        target: clipping.contentLayer
        property: "enabled"
        value: true
        when: clipping.active
        restoreMode: Binding.RestoreBindingOrValue
    }
    property Binding layerSmooth: Binding {
        target: clipping.contentLayer
        property: "smooth"
        value: true
        when: clipping.active
        restoreMode: Binding.RestoreBindingOrValue
    }
    property Binding layerEffect: Binding {
        target: clipping.contentLayer
        property: "effect"
        value: clipping.effectRecipe
        when: clipping.active
        restoreMode: Binding.RestoreBindingOrValue
    }
    property Component effectRecipe: Component {
        MultiEffect {
            objectName: "popupContentMaskEffect"
            maskEnabled: true
            maskSource: clipping.maskOwner.item
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1
            autoPaddingEnabled: false
        }
    }
    property Loader maskOwner: Loader {
        // Never insert the mask into a Menu/Flickable's content tree: those
        // controls may adopt child Items as rows or include them in extents.
        parent: clipping.active ? clipping.frame : null
        active: clipping.active
        visible: false
        width: clipping.content ? clipping.content.width : 0
        height: clipping.content ? clipping.content.height : 0
        sourceComponent: Item {
            objectName: "popupContentRoundedMask"
            layer.enabled: true
            layer.smooth: true
            Rectangle {
                x: clipping.frameRect.x
                y: clipping.frameRect.y
                width: clipping.frameRect.width
                height: clipping.frameRect.height
                radius: clipping.radius
                antialiasing: true
                color: "white"
            }
        }
    }
}
