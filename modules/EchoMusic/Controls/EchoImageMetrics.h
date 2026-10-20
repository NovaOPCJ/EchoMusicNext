#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QSize>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

namespace EchoMusic
{
// Artwork density follows the native item scale chain, not scrolling positions.
// Keep one bounded high-water resolution per visible source to avoid decoding
// smaller copies at every step of a collapsing header.
class EchoImageMetrics : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *item READ item WRITE setItem NOTIFY itemChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QSize pixelSize READ pixelSize NOTIFY metricsChanged)
    Q_PROPERTY(QUrl resolvedSource READ resolvedSource NOTIFY metricsChanged)
public:
    explicit EchoImageMetrics(QObject *parent = nullptr) : QObject(parent) {}
    ~EchoImageMetrics() override;
    QQuickItem *item() const
    {
        return m_item;
    }
    bool active() const
    {
        return m_active;
    }
    QUrl source() const
    {
        return m_source;
    }
    QSize pixelSize() const
    {
        return m_pixels;
    }
    QUrl resolvedSource() const;
    void setItem(QQuickItem *item);
    void setActive(bool active);
    void setSource(const QUrl &source);

signals:
    void itemChanged();
    void activeChanged();
    void sourceChanged();
    void metricsChanged();

private:
    void rebuild();
    void updateSize();
    bool eventFilter(QObject *object, QEvent *event) override;
    QPointer<QQuickItem> m_item;
    QPointer<QQuickWindow> m_window;
    QList<QMetaObject::Connection> m_connections;
    QUrl m_source;
    QSize m_pixels;
    bool m_active = true;
};
} // namespace EchoMusic
