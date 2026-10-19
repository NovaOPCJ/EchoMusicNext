pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Shell
import EchoMusic.PluginSdk 1.0

// Routed plugin pages get the scoped SDK and lifecycle, never the raw router.
Item {
    required property EchoPluginContext plugin
    required property RouteContext context
    readonly property bool pageActive: context.active && plugin.active
}
