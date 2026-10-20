pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import EchoMusic.Theme

Item {
    id: control
    property url source
    property bool circular: false
    property real cornerRadius: circular ? width / 2 : Theme.mediaRadius
    readonly property int status: cover.status
    // Public static artwork for short, frozen compositor transitions.
    readonly property Item transitionArtwork: cover.status === Image.Ready ? cover : placeholder
    property string placeholderGlyph: "music"
    property real placeholderGlyphSize: 20
    property real placeholderOpacity: 0.3
    // Pooled/inactive delegates release decoded images and their mask textures.
    property bool resourceActive: true
    property ThemeColors colors: Theme.colors
    property color placeholderColor: colors.hover
    implicitWidth: 48
    implicitHeight: 48
    EchoImageMetrics {
        id: metrics
        objectName: "coverMetrics"
        item: control
        active: control.resourceActive && control.visible
        source: control.source
    }
    Rectangle {
        id: placeholder
        anchors.fill: parent
        radius: control.cornerRadius
        color: control.placeholderColor
        EchoIcon {
            anchors.centerIn: parent
            width: control.placeholderGlyphSize
            height: control.placeholderGlyphSize
            opacity: control.placeholderOpacity
            color: control.colors.text
            name: control.placeholderGlyph
        }
    }
    Image {
        id: cover
        objectName: "coverImage"
        anchors.fill: parent
        source: metrics.resolvedSource
        asynchronous: true
        sourceSize: metrics.pixelSize
        fillMode: Image.PreserveAspectCrop
        visible: false
    }
    Loader {
        objectName: "coverMaskOwner"
        anchors.fill: parent
        active: control.resourceActive && cover.status === Image.Ready
        // The effect and its texture provider share one lifetime. A pooled
        // delegate must not leave an effect referencing a destroyed mask.
        sourceComponent: Item {
            ShaderEffectSource {
                id: capture
                objectName: "coverCapture"
                sourceItem: cover
                sourceRect: Qt.rect(0, 0, control.width, control.height)
                textureSize: metrics.pixelSize
                visible: false
                live: true
                recursive: false
            }
            Item {
                id: mask
                objectName: "coverRoundedMask"
                anchors.fill: parent
                visible: false
                layer.enabled: true
                layer.smooth: true
                layer.textureSize: metrics.pixelSize
                Rectangle {
                    anchors.fill: parent
                    radius: control.cornerRadius
                    antialiasing: true
                    color: "white"
                }
            }
            MultiEffect {
                objectName: "coverMaskEffect"
                anchors.fill: parent
                source: capture
                // Default thresholds turn the mask's fractional coverage into
                // a hard edge. Preserve Qt Rectangle AA at every cover size.
                maskEnabled: true
                maskThresholdMin: 0.5
                maskSpreadAtMin: 1
                autoPaddingEnabled: false
                maskSource: mask
            }
        }
    }
}
