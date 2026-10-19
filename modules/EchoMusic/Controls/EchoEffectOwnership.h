#pragma once
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Completes QObject ownership for native effect children on the pinned SDK.
/// Qt 6.12 RectangularShadow constructs its default shader before its private
/// q_ptr is assigned, then sets only its visual parent. A visual parent does
/// not own that QObject. Keep the original native rendering/material intact.
class EchoEffectOwnership : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *surface READ surface WRITE setSurface NOTIFY surfaceChanged)
public:
    explicit EchoEffectOwnership(QObject *parent = nullptr) : QObject(parent) {}
    QQuickItem *surface() const
    {
        return m_surface;
    }
    void setSurface(QQuickItem *surface);
signals:
    void surfaceChanged();

private:
    void adoptUnownedChildren();
    QPointer<QQuickItem> m_surface;
    QMetaObject::Connection m_childrenChanged;
};
} // namespace EchoMusic
