pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme

EchoCover {
    id: avatar
    property url identityIcon: ""
    property string identityLabel: ""
    property real identityInset: 1
    circular: true
    placeholderColor: Theme.mutedSurface
    placeholderGlyph: "user"
    // The fallback uses 60% of the avatar at 10% opacity.
    // The sidebar rail overrides opacity to 30%.
    placeholderGlyphSize: width * 0.6
    placeholderOpacity: 0.1
    implicitWidth: 44
    implicitHeight: 44
    // A sibling of the circular cover mask: the corner mark stays unclipped.
    Image {
        id: identityMark
        objectName: "avatarIdentityMark"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: avatar.identityInset
        anchors.bottomMargin: avatar.identityInset
        width: 16
        height: 16
        z: 1
        asynchronous: true
        source: avatar.resourceActive ? avatar.identityIcon : ""
        sourceSize: Qt.size(16 * Screen.devicePixelRatio, 16 * Screen.devicePixelRatio)
        fillMode: Image.PreserveAspectFit
        visible: status === Image.Ready
        Accessible.role: Accessible.Graphic
        Accessible.name: avatar.identityLabel
        HoverHandler {
            id: markHover
        }
        EchoToolTip {
            parent: identityMark
            requestedVisible: markHover.hovered && avatar.identityLabel.length > 0
            text: avatar.identityLabel
        }
    }
}
