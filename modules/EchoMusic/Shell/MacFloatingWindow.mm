#include <QQuickWindow>
#import <Cocoa/Cocoa.h>
namespace EchoMusic
{
void configureMacFloatingPopup(QWindow *popup, QWindow *parent)
{
    if (!popup || !parent || !popup->handle() || !parent->handle())
        return;
    auto *view = reinterpret_cast<NSView *>(popup->winId());
    auto *parentView = reinterpret_cast<NSView *>(parent->winId());
    auto *native = view.window;
    auto *host = parentView.window;
    if (!native || !host)
        return;
    if ([native isKindOfClass:NSPanel.class]) {
        const bool nonactivating = host.styleMask & NSWindowStyleMaskNonactivatingPanel;
        const auto style = nonactivating ? native.styleMask | NSWindowStyleMaskNonactivatingPanel
                                         : native.styleMask & ~NSWindowStyleMaskNonactivatingPanel;
        if (native.styleMask != style)
            native.styleMask = style;
    }
    if (native.hidesOnDeactivate)
        native.hidesOnDeactivate = NO;
    const auto behavior = host.collectionBehavior | NSWindowCollectionBehaviorFullScreenAuxiliary;
    if (native.collectionBehavior != behavior)
        native.collectionBehavior = behavior;
    // Qt's Popup.Window flags reset the panel to its own normal level (8) even
    // when the pinned host has been raised to screen-saver level (1000).
    // Preserve Qt's base when unpinned, and order strictly above the real parent.
    // Same precedence as the locked Qt Cocoa backend, including native hints.
    NSInteger base = popup->type() == Qt::Tool ? NSFloatingWindowLevel : NSNormalWindowLevel;
    if ((popup->type() & Qt::Popup) == Qt::Popup)
        base = NSPopUpMenuWindowLevel;
    if (popup->flags() & Qt::WindowStaysOnTopHint)
        base = NSModalPanelWindowLevel;
    if (popup->type() == Qt::ToolTip)
        base = NSScreenSaverWindowLevel;
    const NSInteger level = qMax(base, host.level + 1);
    if (native.level != level)
        native.level = level;
    if (native.visible)
        [native orderWindow:NSWindowAbove relativeTo:host.windowNumber];
}

void configureMacFloating(QQuickWindow *window, bool onTop, bool desktop)
{
    if (!window)
        return;
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *native = view.window;
    if (!native)
        return;
    if ([native isKindOfClass:NSPanel.class]) {
        // Qt::Tool already owns an NSPanel. Make only pinned overlays
        // nonactivating so they can join another app's fullscreen without
        // changing the whole application's identity (and its Dock entry).
        native.styleMask = onTop ? native.styleMask | NSWindowStyleMaskNonactivatingPanel
                                 : native.styleMask & ~NSWindowStyleMaskNonactivatingPanel;
        static_cast<NSPanel *>(native).becomesKeyOnlyIfNeeded = onTop;
    }
    native.hidesOnDeactivate = NO;
    // Cross-application fullscreen also needs the nonactivating panel above.
    // All used APIs predate the 14.4 baseline; the explicit cross-application
    // behavior is available since 13.0. Do not transform the application process.
    // Set collection behavior before level, as AppKit validates them together.
    native.collectionBehavior = onTop ? NSWindowCollectionBehaviorCanJoinAllSpaces |
                                            NSWindowCollectionBehaviorFullScreenAuxiliary |
                                            NSWindowCollectionBehaviorCanJoinAllApplications
                                      : NSWindowCollectionBehaviorDefault;
    native.level = onTop ? NSScreenSaverWindowLevel : NSNormalWindowLevel;
    native.hasShadow = !desktop;
}
} // namespace EchoMusic
