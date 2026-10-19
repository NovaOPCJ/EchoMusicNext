#import <Cocoa/Cocoa.h>
#include <QQuickWindow>
#include <QRectF>

namespace EchoMusic
{
void setMacDockHidden(bool hidden)
{
    [NSApp setActivationPolicy:hidden ? NSApplicationActivationPolicyAccessory
                                      : NSApplicationActivationPolicyRegular];
}
void configureMacWindow(QQuickWindow *window)
{
    // Qt owns the native window and its standard buttons. Keep their default
    // AppKit layout; only suppress the redundant title over the custom chrome.
    auto *content = reinterpret_cast<NSView *>(window->winId());
    content.window.titleVisibility = NSWindowTitleHidden;
}
QRectF macCaptionBounds(QQuickWindow *window)
{
    if (!window || !window->handle())
        return {};
    auto *content = reinterpret_cast<NSView *>(window->winId());
    auto *native = content.window;
    if (!native || (native.styleMask & NSWindowStyleMaskFullScreen))
        return {};
    QRectF bounds;
    const NSWindowButton kinds[]{NSWindowCloseButton, NSWindowMiniaturizeButton,
                                 NSWindowZoomButton};
    for (const auto kind : kinds) {
        auto *button = [native standardWindowButton:kind];
        if (!button.superview)
            return {};
        const auto rect = [button convertRect:button.bounds toView:content];
        const auto top =
            content.isFlipped ? rect.origin.y : content.bounds.size.height - NSMaxY(rect);
        bounds = bounds.united(QRectF(rect.origin.x, top, rect.size.width, rect.size.height));
    }
    // QWindow geometry is in Qt DIP; AppKit coordinates are window points.
    // Application UI zoom is applied later by QML, independently of this scale.
    const auto pointScale = window->width() > 0 ? content.bounds.size.width / window->width() : 1.;
    return pointScale > 0 ? QRectF(bounds.topLeft() / pointScale, bounds.size() / pointScale)
                          : QRectF{};
}
} // namespace EchoMusic
