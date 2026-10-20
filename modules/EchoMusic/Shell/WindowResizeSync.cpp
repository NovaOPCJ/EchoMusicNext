#include "WindowResizeSync.h"
#include <QCoreApplication>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QResizeEvent>
#include <QSGRendererInterface>
#include <utility>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <commctrl.h>
#endif

namespace EchoMusic
{
struct WindowResizeSync::Native
{
#ifdef Q_OS_WIN
    WindowResizeSync *owner;
    HWND handle = nullptr;
    explicit Native(WindowResizeSync *sync) : owner(sync) {}
    ~Native()
    {
        detach();
    }
    void detach()
    {
        if (handle)
            RemoveWindowSubclass(handle, procedure, reinterpret_cast<UINT_PTR>(this));
        handle = nullptr;
    }
    void attach(HWND window)
    {
        detach();
        if (window && SetWindowSubclass(window, procedure, reinterpret_cast<UINT_PTR>(this),
                                        reinterpret_cast<DWORD_PTR>(this)))
            handle = window;
        else
            qWarning("Cannot install native resize presentation sync");
    }
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
                                      UINT_PTR, DWORD_PTR data)
    {
        auto *native = reinterpret_cast<Native *>(data);
        const QPointer<WindowResizeSync> sync(native->owner);
        if (message == WM_ENTERSIZEMOVE)
            sync->setInteractiveResize(true);
        else if (message == WM_EXITSIZEMOVE || message == WM_CANCELMODE || message == WM_NCDESTROY)
            sync->setInteractiveResize(false);
        // Qt must first update its platform geometry. Never consume WM_SIZE or
        // replace its window procedure; preserve the subclass chain.
        const auto result = DefSubclassProc(window, message, wParam, lParam);
        if (!sync)
            return result;
        if (message == WM_NCDESTROY) {
            if (native->handle == window)
                native->detach();
            return result;
        }
        if (message == WM_SIZE && native->handle == window && wParam != SIZE_MINIMIZED &&
            sync->m_interactive && !sync->m_delivering && sync->m_window &&
            sync->m_window->isVisible() && sync->m_window->isExposed()) {
            const auto api = sync->m_window->rendererInterface()->graphicsApi();
            if (!QSGRendererInterface::isApiRhiBased(api) || api == QSGRendererInterface::Null)
                return result;
            sync->m_delivering = true;
            // Qt's Windows WM_PAINT handler synchronously flushes geometry and
            // expose events. Force that normal paint before WM_SIZE returns,
            // rather than letting DWM stretch the old swapchain frame meanwhile.
            const bool painted = RedrawWindow(window, nullptr, nullptr,
                                              RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE |
                                                  RDW_NOCHILDREN) != FALSE;
            if (sync) {
                if (!painted && !sync->m_warned) {
                    sync->m_warned = true;
                    qWarning("Native resize paint failed: error=%lu", GetLastError());
                }
                if (sync->m_interactive && sync->m_window && sync->m_window->isVisible() &&
                    sync->m_window->isExposed())
                    sync->waitForCompositor();
                if (sync)
                    sync->m_delivering = false;
            }
        }
        return result;
    }
#endif
};
WindowResizeSync::WindowResizeSync(QQuickWindow *window, std::function<void()> compositorWait)
    : QObject(window), m_window(window), m_compositorWait(std::move(compositorWait))
{
    Q_ASSERT(window);
    window->installEventFilter(this);
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() == QStringLiteral("windows")) {
        m_native = std::make_unique<Native>(this);
        m_native->attach(reinterpret_cast<HWND>(window->winId()));
    }
#endif
}
WindowResizeSync::~WindowResizeSync() = default;
void WindowResizeSync::setInteractiveResize(bool active)
{
    m_interactive = active;
    if (!active)
        m_warned = false;
}
void WindowResizeSync::waitForCompositor()
{
    if (m_compositorWait) {
        m_compositorWait();
        return;
    }
#ifdef Q_OS_WIN
    BOOL composing = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&composing)) && composing) {
        const auto result = DwmFlush();
        if (FAILED(result) && !m_warned) {
            m_warned = true;
            qWarning("Resize composition sync failed: HRESULT=0x%08lx", DWORD(result));
        }
    }
#endif
}
bool WindowResizeSync::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window || !m_window)
        return false;
#ifdef Q_OS_WIN
    if (event->type() == QEvent::PlatformSurface && m_native) {
        const auto type = static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType();
        if (type == QPlatformSurfaceEvent::SurfaceCreated)
            m_native->attach(reinterpret_cast<HWND>(m_window->winId()));
        else {
            m_native->detach();
            setInteractiveResize(false);
        }
    }
#endif
    if (event->type() == QEvent::Hide)
        setInteractiveResize(false);
    if (event->type() != QEvent::Resize || !m_interactive || m_delivering ||
        !m_window->isVisible() || !m_window->isExposed())
        return false;
#ifdef Q_OS_WIN
    if (m_native && m_native->handle)
        return false; // The post-WM_SIZE path owns native synchronous painting.
#endif
    const auto *resize = static_cast<QResizeEvent *>(event);
    const auto api = m_window->rendererInterface()->graphicsApi();
    if (resize->size().isEmpty() || resize->size() == resize->oldSize() ||
        !QSGRendererInterface::isApiRhiBased(api) || api == QSGRendererInterface::Null)
        return false;
    const QPointer<WindowResizeSync> guard(this);
    const QPointer<QQuickWindow> window = m_window;
    m_delivering = true;
    // First let QQuickWindow update its contentItem and normal resize handlers.
    // The guard prevents redispatch through this filter. Consume the outer event
    // so QQuickWindow handles the actual resize exactly once.
    QCoreApplication::sendEvent(window, event);
    if (!guard || !window)
        return true;
    if (window->isVisible() && window->isExposed()) {
        // Qt 6.12's public expose path completes polish/sync/render/present.
        // In threaded mode it waits until endFrame, unlike a plain update().
        // Complete this before returning from delivery of the Qt resize event.
        QExposeEvent expose(QRegion(QRect(QPoint(), window->size())));
        QCoreApplication::sendEvent(window, &expose);
        if (!guard || !window)
            return true;
        if (!m_interactive || !window->isVisible() || !window->isExposed()) {
            m_delivering = false;
            return true;
        }
        waitForCompositor();
    }
    if (guard)
        m_delivering = false;
    return true;
}
} // namespace EchoMusic
