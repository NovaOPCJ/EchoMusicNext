#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickWindow>
#include <functional>
#include <memory>

namespace EchoMusic
{
/// Coordinates and verifies new-size frame submission during modal resize.
/// Submission is not proof of DWM's displayed result; diagnostics distinguish it.
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
    bool submitFrame();
    bool nativeGeometryMatchesWindow() const;
    bool frameMatchesWindow(quint64 after) const;
    struct Native;
    struct Frames;
    std::unique_ptr<Native> m_native;
    std::shared_ptr<Frames> m_frames;
    QPointer<QQuickWindow> m_window;
    std::function<void()> m_compositorWait;
    bool m_interactive = false;
    bool m_delivering = false;
    bool m_warned = false;
    bool m_diagnostics = false;
    quint64 m_nativeSteps = 0, m_qtSteps = 0, m_explicitExposes = 0, m_incompleteFrames = 0;
    qint64 m_maximumPaintNs = 0, m_maximumCompositionNs = 0;
    QSize m_nativePixels;
};
} // namespace EchoMusic
