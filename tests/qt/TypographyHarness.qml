pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Controls
import EchoMusic.Theme

// Test host only: production controls outside ApplicationWindow also receive
// the complete font policy, including locally overridden sizes and weights.
Window {
    id: window
    width: 760
    height: 620
    visible: true
    color: Theme.main
    Column {
        x: 24
        y: 24
        spacing: 12
        Repeater {
            model: [400, 500, 600, 650, 700, 800, 900]
            Label {
                required property int modelData
                objectName: "weight" + modelData
                text: "DY4AGX 茶汤《不知道寄还哪地方》"
                color: Theme.text
                font: Theme.spacedFont(20, modelData, -0.2)
            }
        }
        EchoSoftButton {
            objectName: "typographySoftButton"
            text: "查看账号 Account"
            font: Theme.font(14, Theme.semiboldWeight)
        }
        EchoButton {
            objectName: "typographyFormButton"
            text: "立即登录 Login"
            font: Theme.font(16, Theme.blackWeight)
        }
        EchoTextField {
            objectName: "typographyInput"
            width: 400
            text: "茶汤 Account"
            font: Theme.font(14, Theme.boldWeight)
        }
        EchoButtonContent {
            objectName: "typographyButtonContent"
            width: 400
            height: 30
            text: "查看结果 Result"
        }
        Text {
            objectName: "typographyDecoratedText"
            text: "中文 English"
            font: Theme.decoratedFont(13, Theme.semiboldWeight, 0.5, true, true)
            color: Theme.text
        }
    }
}
