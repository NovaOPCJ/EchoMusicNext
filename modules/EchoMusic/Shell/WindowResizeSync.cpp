#include "WindowResizeSync.h"
#include <QCoreApplication>
#include <QExposeEvent>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QResizeEvent>
#include <QSGRendererInterface>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <atomic>
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
namespace
{
Q_LOGGING_CATEGORY(resizeLog, "echo.presentation.resize")
quint64 packedSize(QSize size)
{
    return (quint64(quint32(size.width())) << 32) | quint32(size.height());
}
QSize unpackedSize(quint64 packed)
{
    return {int(packed >> 32), int(quint32(packed))};
}
} // namespace
struct WindowResizeSync::Frames
{
    std::atomic<quint64> submitted{0}, synchronizingSize{0}, submittedSize{0};
};
struct WindowResizeSync::Native
{
#ifdef Q_OS_WIN
    WindowResizeSync *owner;
    HWND handle = nullptr;
    unsigned sizeDeliveryDepth = 0;
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
        if (message == WM_SIZE)
            ++native->sizeDeliveryDepth;
        const auto result = DefSubclassProc(window, message, wParam, lParam);
        if (!sync)
            return result;
        if (message == WM_SIZE && --native->sizeDeliveryDepth != 0)
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
            ++sync->m_nativeSteps;
            RECT client;
            if (GetClientRect(window, &client))
                sync->m_nativePixels = {client.right - client.left, client.bottom - client.top};
            const auto before = sync->m_frames->submitted.load(std::memory_order_acquire);
            QElapsedTimer paintTime;
            paintTime.start();
            // Qt's Windows WM_PAINT handler synchronously flushes geometry and
            // expose events. Force that normal paint before WM_SIZE returns,
            // rather than letting DWM stretch the old swapchain frame meanwhile.
            const bool painted = RedrawWindow(window, nullptr, nullptr,
                                              RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE |
                                                  RDW_NOCHILDREN) != FALSE;
            if (sync) {
                sync->m_maximumPaintNs = qMax(sync->m_maximumPaintNs, paintTime.nsecsElapsed());
                if (!painted && !sync->m_warned) {
                    sync->m_warned = true;
                    qWarning("Native resize paint failed: error=%lu", GetLastError());
                }
                // WM_PAINT success is not a frame fence. If Qt did not submit
                // the new logical size, explicitly take its public expose path.
                if (!sync->frameMatchesWindow(before))
                    sync->submitFrame();
                if (!sync)
                    return result;
                const bool submitted = sync->frameMatchesWindow(before);
                if (!submitted) {
                    ++sync->m_incompleteFrames;
                    if (sync->nativeGeometryMatchesWindow() && !sync->m_warned) {
                        sync->m_warned = true;
                        qCWarning(resizeLog) << "Resize paint returned without a matching frame";
                    }
                }
                if (submitted && sync->m_interactive && sync->m_window &&
                    sync->m_window->isVisible() && sync->m_window->isExposed())
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
    : QObject(window), m_frames(std::make_shared<Frames>()), m_window(window),
      m_compositorWait(std::move(compositorWait)),
      m_diagnostics(qEnvironmentVariableIntValue("ECHO_RENDER_DIAGNOSTICS") == 1)
{
    Q_ASSERT(window);
    window->installEventFilter(this);
    const auto frames = m_frames;
    connect(
        window, &QQuickWindow::beforeSynchronizing, this,
        [window, frames] {
            // Qt blocks the GUI during synchronization, so its logical size can be
            // sampled here. Never read GUI geometry from afterFrameEnd.
            frames->synchronizingSize.store(packedSize(window->size()), std::memory_order_relaxed);
        },
        Qt::DirectConnection);
    connect(
        window, &QQuickWindow::afterFrameEnd, this,
        [frames] {
            frames->submittedSize.store(frames->synchronizingSize.load(std::memory_order_relaxed),
                                        std::memory_order_relaxed);
            frames->submitted.fetch_add(1, std::memory_order_release);
        },
        Qt::DirectConnection);
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
    if (m_interactive == active)
        return;
    m_interactive = active;
    if (active) {
        m_nativeSteps = m_qtSteps = m_explicitExposes = m_incompleteFrames = 0;
        m_maximumPaintNs = m_maximumCompositionNs = 0;
        m_nativePixels = {};
        if (m_diagnostics && m_window)
            qCInfo(resizeLog) << "Resize started"
                              << "api=" << m_window->rendererInterface()->graphicsApi()
                              << "dpr=" << m_window->effectiveDevicePixelRatio()
                              << "qt-size=" << m_window->size();
    } else {
        if (m_diagnostics)
            qCInfo(resizeLog) << "Resize completed" << "native-steps=" << m_nativeSteps
                              << "late-qt-steps=" << m_qtSteps
                              << "explicit-exposes=" << m_explicitExposes
                              << "incomplete-frames=" << m_incompleteFrames
                              << "max-paint-ms=" << m_maximumPaintNs / 1000000.
                              << "max-composition-ms=" << m_maximumCompositionNs / 1000000.
                              << "native-pixels=" << m_nativePixels
                              << "qt-size=" << (m_window ? m_window->size() : QSize())
                              << "submitted-size=" << unpackedSize(m_frames->submittedSize.load());
        m_warned = false;
    }
}
bool WindowResizeSync::frameMatchesWindow(quint64 after) const
{
    return m_window && nativeGeometryMatchesWindow() &&
           m_frames->submitted.load(std::memory_order_acquire) > after &&
           m_frames->submittedSize.load(std::memory_order_relaxed) == packedSize(m_window->size());
}
bool WindowResizeSync::nativeGeometryMatchesWindow() const
{
#ifdef Q_OS_WIN
    if (m_native && m_native->handle && m_window) {
        RECT client;
        if (!GetClientRect(m_native->handle, &client))
            return false;
        const auto expected = m_window->size() * m_window->effectiveDevicePixelRatio();
        // Qt's DIP geometry is integral, so fractional DPR can round by one
        // physical pixel. Larger differences mean geometry is still queued.
        return qAbs(expected.width() - (client.right - client.left)) <= 1 &&
               qAbs(expected.height() - (client.bottom - client.top)) <= 1;
    }
#endif
    return true;
}
bool WindowResizeSync::submitFrame()
{
    const QPointer<WindowResizeSync> guard(this);
    const QPointer<QQuickWindow> window = m_window;
    if (!window || !m_interactive || !window->isVisible() || !window->isExposed() ||
        !nativeGeometryMatchesWindow())
        return false;
    const auto before = m_frames->submitted.load(std::memory_order_acquire);
    ++m_explicitExposes;
    QExposeEvent expose(QRegion(QRect(QPoint(), window->size())));
    QCoreApplication::sendEvent(window, &expose);
    return guard && frameMatchesWindow(before);
}
void WindowResizeSync::waitForCompositor()
{
    QElapsedTimer elapsed;
    elapsed.start();
    if (m_compositorWait) {
        const QPointer<WindowResizeSync> guard(this);
        m_compositorWait();
        if (guard)
            m_maximumCompositionNs = qMax(m_maximumCompositionNs, elapsed.nsecsElapsed());
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
    m_maximumCompositionNs = qMax(m_maximumCompositionNs, elapsed.nsecsElapsed());
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
#ifdef Q_OS_WIN
    // A resize delivered by DefSubclassProc is handled by the outer native
    // paint path. Only genuinely later Qt geometry needs this fallback, or
    // the same WM_SIZE would submit/wait twice.
    if (m_native && m_native->sizeDeliveryDepth)
        return false;
#endif
    if (event->type() != QEvent::Resize || !m_interactive || m_delivering ||
        !m_window->isVisible() || !m_window->isExposed())
        return false;
    // Geometry can be delivered after WM_SIZE/WM_PAINT. Do not skip that later
    // Qt resize just because the native hook exists; it is the new-size frame's
    // last synchronization point. RedrawWindow delivery is already guarded.
    const auto *resize = static_cast<QResizeEvent *>(event);
    const auto api = m_window->rendererInterface()->graphicsApi();
    if (resize->size().isEmpty() || resize->size() == resize->oldSize() ||
        !QSGRendererInterface::isApiRhiBased(api) || api == QSGRendererInterface::Null)
        return false;
    const QPointer<WindowResizeSync> guard(this);
    const QPointer<QQuickWindow> window = m_window;
    m_delivering = true;
    ++m_qtSteps;
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
        const bool submitted = submitFrame();
        if (!guard || !window)
            return true;
        if (!m_interactive || !window->isVisible() || !window->isExposed()) {
            m_delivering = false;
            return true;
        }
        if (!submitted)
            ++m_incompleteFrames;
        // Never block composition on an old-size frame while Qt's newer
        // geometry is still queued; the matching resize event will do this.
        if (submitted)
            waitForCompositor();
    }
    if (guard)
        m_delivering = false;
    return true;
}
} // namespace EchoMusic
