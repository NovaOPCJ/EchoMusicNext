pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Playback

Item {
    id: dots
    required property LyricInterlude animation
    required property real fontSize
    required property real pageHeight
    required property color color
    required property bool motionEnabled
    // CSS content height plus .4em vertical padding; the outer row adds its
    // own .4em margin. The three dots stay a distinct layout/focus row.
    implicitWidth: fontSize * 3.26
    implicitHeight: Math.max(fontSize * .5, Math.min(pageHeight * .01, fontSize * 3)) + fontSize * .8
    opacity: animation.opacity
    scale: motionEnabled ? animation.scaleFactor : 1
    transformOrigin: Item.Center
    Repeater {
        model: 3
        Rectangle {
            required property int index
            objectName: "lyricInterludeDot" + index
            x: dots.fontSize * (1 + index * .48)
            y: (dots.height - height) / 2
            width: dots.fontSize * .3
            height: width
            radius: width / 2
            color: dots.color
            opacity: !dots.motionEnabled ? .9 : index === 0 ? dots.animation.firstOpacity : index === 1 ? dots.animation.secondOpacity : dots.animation.thirdOpacity
        }
    }
}
