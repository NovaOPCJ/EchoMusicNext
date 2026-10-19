pragma ComponentBehavior: Bound
import QtQuick
import EchoMusic.Shell

Item {
    id: host
    required property RouteController router
    required property ShellSettings settings
    clip: true
    Repeater {
        // Only mounted contexts (one active + bounded LRU), never all routes.
        model: host.router
        delegate: Loader {
            id: pageLoader
            required property RouteContext pageContext
            anchors.fill: parent
            visible: pageContext.active
            enabled: visible
            z: visible ? 1 : 0
            asynchronous: true
            Component.onCompleted: setSource(pageContext.component, Object.assign({}, pageContext.initialProperties, pageContext.pluginOwned ? {
                context: pageContext
            } : {
                context: pageContext,
                router: host.router,
                settings: host.settings
            }))
        }
    }
}
