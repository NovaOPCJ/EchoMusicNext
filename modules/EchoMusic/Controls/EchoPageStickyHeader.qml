pragma ComponentBehavior: Bound
import QtQuick

// The page owns the visual layer; the caller reserves normal-flow space using
// its view's topMargin. Controls remain alive outside recycled delegates.
Item {
    id: header
    required property EchoPageScrollContainer container
    property real flowPosition: 0
    property real stickyTop: 0
    property bool sticky: true
    property real leftInset: 0
    property real rightInset: 0
    parent: sticky ? container.stickyLayer : container.flowHeaderLayer
    x: leftInset
    width: Math.max(0, container.width - leftInset - rightInset)
    y: sticky ? Math.max(stickyTop, flowPosition - container.scrollTop) : flowPosition - container.scrollTop
}
