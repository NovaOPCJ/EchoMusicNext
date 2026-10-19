pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Shell

// Shared contract for every routed page. Pause page-local work when pageActive
// becomes false; destruction releases subscriptions. Cached visual state stays here.
Item {
    required property RouteContext context
    required property RouteController router
    required property ShellSettings settings
    readonly property bool pageActive: context.active
}
