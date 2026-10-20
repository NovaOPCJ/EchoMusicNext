#pragma once
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Marks the current scene below consuming popups. This is only a source marker,
/// not a texture/layer. Activating a page scope replaces the window's source;
/// deactivation/destruction restores the preceding scene without sampling Overlay.
class EchoBackdropRoot : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool samplingActive READ samplingActive WRITE setSamplingActive NOTIFY
                   samplingActiveChanged)
public:
    explicit EchoBackdropRoot(QQuickItem *parent = nullptr);
    ~EchoBackdropRoot() override;
    bool samplingActive() const
    {
        return m_samplingActive;
    }
    void setSamplingActive(bool active);
signals:
    void samplingActiveChanged();

private:
    void registerWindow(QQuickWindow *window);
    QPointer<QQuickWindow> m_window;
    bool m_samplingActive = true;
};
/// Per-surface, per-window geometry adapter. Observes both item ancestry chains
/// instead of polling every frame; no shared window state or FBO is retained.
class EchoBackdropSampler : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *surface READ surface WRITE setSurface NOTIFY surfaceChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(qreal padding READ padding WRITE setPadding NOTIFY changed)
    Q_PROPERTY(QQuickItem *source READ source NOTIFY changed)
    Q_PROPERTY(QRectF sourceRect READ sourceRect NOTIFY changed)
public:
    explicit EchoBackdropSampler(QObject *parent = nullptr);
    QQuickItem *surface() const
    {
        return m_surface;
    }
    void setSurface(QQuickItem *surface);
    bool enabled() const
    {
        return m_enabled;
    }
    void setEnabled(bool enabled);
    qreal padding() const
    {
        return m_padding;
    }
    void setPadding(qreal padding);
    QQuickItem *source() const;
    QRectF sourceRect() const;
signals:
    void surfaceChanged();
    void changed();

private:
    void observe();
    bool eventFilter(QObject *watched, QEvent *event) override;
    QPointer<QQuickItem> m_surface;
    QPointer<QQuickWindow> m_window;
    QList<QMetaObject::Connection> m_connections;
    bool m_enabled = false;
    qreal m_padding = 24;
};
} // namespace EchoMusic
