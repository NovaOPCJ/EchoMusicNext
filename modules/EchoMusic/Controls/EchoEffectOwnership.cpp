#include "EchoEffectOwnership.h"
#include <QQmlEngine>
namespace EchoMusic
{
void EchoEffectOwnership::setSurface(QQuickItem *surface)
{
    if (m_surface == surface)
        return;
    disconnect(m_childrenChanged);
    m_surface = surface;
    if (surface)
        m_childrenChanged = connect(surface, &QQuickItem::childrenChanged, this,
                                    &EchoEffectOwnership::adoptUnownedChildren);
    adoptUnownedChildren();
    emit surfaceChanged();
}
void EchoEffectOwnership::adoptUnownedChildren()
{
    if (!m_surface)
        return;
    // The public material getter also exposes the native default before it
    // has a window/size and therefore before it has any visual parent.
    auto *material = m_surface->property("material").value<QQuickItem *>();
    if (material && material != m_surface && !material->parent() &&
        QQmlEngine::objectOwnership(material) == QQmlEngine::CppOwnership)
        material->setParent(m_surface);
    // Used only by the shared native shadow. Never take an already-owned or
    // JS-owned QML item, and never change visual parenting or layout geometry.
    for (auto *child : m_surface->childItems()) {
        if (!child->parent() && QQmlEngine::objectOwnership(child) == QQmlEngine::CppOwnership)
            child->setParent(m_surface);
    }
}
} // namespace EchoMusic
