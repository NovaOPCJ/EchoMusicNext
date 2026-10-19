pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme
import EchoMusic.Shell

Item {
    id: shortcuts
    required property SidebarLayoutModel layout
    required property RouteController router
    required property bool collapsed
    property bool resourceActive: true
    signal addRequested
    // Natural width establishes the inline header height before the parent
    // assigns its final expanded or collapsed width.
    implicitWidth: 194
    implicitHeight: grid.implicitHeight
    // A bounded positioner owns every card coordinate. Its move transition
    // animates incremental model moves without fighting a Layout's geometry.
    Flow {
        id: grid
        width: parent.width
        spacing: 8
        move: Transition {
            NumberAnimation {
                properties: "x,y"
                duration: Theme.store.motionEnabled ? 160 : 0
                easing.type: Easing.BezierSpline
                easing.bezierCurve: [0.25, 0.1, 0.25, 1, 1, 1]
            }
        }
        Repeater {
            id: cards
            // The built-in set is bounded; never project the user's unbounded
            // playlist catalog through this positioner.
            model: shortcuts.layout.shortcutModel
            delegate: SidebarShortcutCard {
                required property int index
                required property string shortcutKey
                required property string shortcutTitle
                required property string shortcutRoute
                required property string shortcutGlyph
                layout: shortcuts.layout
                router: shortcuts.router
                entryKey: shortcutKey
                routeName: shortcutRoute
                title: shortcutTitle
                glyph: shortcutGlyph
                entryIndex: index
                collapsed: shortcuts.collapsed
                resourceActive: shortcuts.resourceActive
                dragLayer: shortcuts
                width: shortcuts.collapsed ? grid.width : Math.max(0, (grid.width - grid.spacing) / 2)
                height: shortcuts.collapsed ? 40 : 46
                onReorderRequested: (key, target, after) => {
                    shortcuts.layout.moveShortcutBefore(key, target, after);
                    restoreKeyboardFocus(key);
                }
                function restoreKeyboardFocus(key) {
                    Qt.callLater(() => {
                        for (let i = 0; i < cards.count; ++i) {
                            const candidate = cards.itemAt(i);
                            if (candidate && candidate.entryKey === key && candidate.activation.enabled)
                                candidate.activation.forceActiveFocus(Qt.TabFocusReason);
                        }
                    });
                }
                onRemoved: (previous, keyboard) => {
                    if (!keyboard)
                        return;
                    Qt.callLater(() => {
                        const next = cards.itemAt(Math.min(previous, cards.count - 1));
                        if (next && next.activation.enabled)
                            next.activation.forceActiveFocus(Qt.TabFocusReason);
                        else
                            add.forceActiveFocus(Qt.TabFocusReason);
                    });
                }
            }
        }
        EchoIconButton {
            id: add
            objectName: "sidebarShortcutAdd"
            glyph: "plus"
            focusPolicy: Qt.StrongFocus
            text: qsTr("添加快捷卡片")
            width: shortcuts.collapsed || cards.count % 2 === 0 ? grid.width : Math.max(0, (grid.width - grid.spacing) / 2)
            height: shortcuts.collapsed ? 40 : cards.count % 2 === 0 ? 28 : 46
            padding: !shortcuts.collapsed && cards.count % 2 === 0 ? 0 : 8
            foreground: Theme.secondaryText
            contentItem: Item {
                EchoIcon {
                    anchors.centerIn: parent
                    width: 25
                    height: 25
                    name: "plus"
                    color: add.foreground
                }
            }
            onClicked: shortcuts.addRequested()
            background: Canvas {
                readonly property color outlineColor: Theme.colors.strongBorder
                onOutlineColorChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.fillStyle = add.hovered ? Theme.hover : "transparent";
                    ctx.strokeStyle = outlineColor;
                    ctx.lineWidth = 1;
                    ctx.setLineDash([3, 3]);
                    ctx.beginPath();
                    ctx.roundedRect(0.5, 0.5, width - 1, height - 1, Theme.cardRadius, Theme.cardRadius);
                    ctx.fill();
                    ctx.stroke();
                }
                Connections {
                    target: add
                    function onHoveredChanged() {
                        add.background.requestPaint();
                    }
                }
            }
        }
    }
}
