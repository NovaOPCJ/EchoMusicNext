pragma ComponentBehavior: Bound
import QtQuick

// A typed presentation palette, shared by host content and floating surfaces.
// Services own the resolved values; components consume this scope without
// probing an untyped QObject or parsing arbitrary token strings.
QtObject {
    required property color text
    required property color tone
    required property color secondaryText
    required property color elevated
    required property color card
    required property color border
    required property color accent
    required property color accentText
    readonly property color iconMain: Qt.rgba(text.r, text.g, text.b, 0.8)
    readonly property color iconSecondary: Qt.rgba(text.r, text.g, text.b, 0.6)
    readonly property color surfaceOutline: Qt.rgba(tone.r, tone.g, tone.b, 0.1)
    readonly property color strongBorder: Qt.rgba(tone.r, tone.g, tone.b, 0.18)
    readonly property color controlBorder: Qt.rgba(border.r, border.g, border.b, border.a * 0.76)
    readonly property color subtleBorder: Qt.rgba(border.r, border.g, border.b, 0.72)
    readonly property color hover: Qt.rgba(tone.r, tone.g, tone.b, 0.1)
    readonly property color mutedSurface: Qt.rgba(tone.r, tone.g, tone.b, 0.05)
    readonly property color neutralPressed: Qt.rgba(tone.r, tone.g, tone.b, 0.14)
    readonly property color selected: Qt.rgba(accent.r, accent.g, accent.b, 0.12)
    readonly property color hotCard: Qt.rgba(text.r, text.g, text.b, 0.03)
    // Danger colors mix with the actual content or floating text,
    // independently of the user accent.
    readonly property color success: "#10b981"
    // Fixed semantic warning color.
    readonly property color warning: "#f59e0b"
    readonly property color danger: "#ef4444"
    readonly property color dangerText: Qt.rgba(danger.r * 0.76 + text.r * 0.24, danger.g * 0.76 + text.g * 0.24, danger.b * 0.76 + text.b * 0.24, 1)
}
