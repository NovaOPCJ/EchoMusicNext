pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme

// Rendering fixture for every production text/tag recipe, never a product UI.
Window {
    id: window
    width: 780
    height: Math.max(420, samples.height * sampleScale + 60)
    visible: true
    color: Theme.main
    property string sampleText: "一天"
    property real availableWidth: 720
    property real sampleScale: 1
    property bool showTags: true
    Column {
        id: samples
        scale: window.sampleScale
        transformOrigin: Item.TopLeft
        x: 30
        y: 30
        spacing: 25
        Repeater {
            model: [
                {
                    size: 20,
                    weight: 700,
                    height: 26
                },
                {
                    size: 11,
                    weight: 400,
                    height: 16
                },
                {
                    size: 14,
                    weight: 600,
                    height: 21
                },
                {
                    size: 14,
                    weight: 600,
                    height: 21
                },
                {
                    size: 18,
                    weight: 700,
                    height: 28
                }
            ]
            delegate: EchoTextTagRow {
                id: sample
                required property int index
                required property var modelData
                objectName: "alignmentRow-" + index
                width: window.availableWidth
                text: window.sampleText
                font: Theme.font(modelData.size, modelData.weight)
                lineHeight: modelData.height
                color: "#ed376a"
                spacing: 8
                wrap: index === 0 || index === 4
                EchoTag {
                    objectName: "alignmentTag"
                    visible: window.showTags && sample.index < 3
                    text: "超级VIP"
                    tone: EchoTag.Custom
                    customTone: "#f97316"
                }
                EchoBadge {
                    objectName: "alignmentBadge"
                    visible: window.showTags && sample.index >= 3
                    count: 120
                    tone: EchoBadge.Muted
                }
                EchoTag {
                    objectName: "alignmentRegion"
                    visible: window.showTags && (sample.index === 0 || sample.index === 2)
                    text: "四川"
                    tone: EchoTag.Muted
                }
            }
        }
    }
}
