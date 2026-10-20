#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <functional>
#include <memory>

namespace EchoMusic
{
/// Completes the new-size Quick frame before returning to Windows' modal resize.
/// No render-loop/backend changes and no work while the window is idle or moving.
class WindowResizeSync final : public QObject
{
public:
    explicit WindowResizeSync(QQuickWindow *window, std::function<void()> compositorWait = {});
    ~WindowResizeSync() override;
    // Platform state, also usable by the native rendering regression harness.
    void setInteractiveResize(bool active);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void waitForCompositor();
    struct Native;
    std::unique_ptr<Native> m_native;
    QPointer<QQuickWindow> m_window;
    std::function<void()> m_compositorWait;
    bool m_interactive = false;
    bool m_delivering = false;
    bool m_warned = false;
};
} // namespace EchoMusic
