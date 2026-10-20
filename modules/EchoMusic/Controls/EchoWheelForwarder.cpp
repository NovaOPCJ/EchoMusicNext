#include "EchoWheelForwarder.h"
#include <QCoreApplication>
#include <QWheelEvent>

namespace EchoMusic
{
void EchoWheelForwarder::setWheelView(QQuickItem *view)
{
    if (m_view == view)
        return;
    m_view = view;
    emit wheelViewChanged();
}
void EchoWheelForwarder::setForwardOutsideViewOnly(bool value)
{
    if (m_outsideOnly == value)
        return;
    m_outsideOnly = value;
    emit forwardOutsideViewOnlyChanged();
}
void EchoWheelForwarder::setForwardingEnabled(bool value)
{
    if (m_forwardingEnabled == value)
        return;
    m_forwardingEnabled = value;
    emit forwardingEnabledChanged();
}
void EchoWheelForwarder::wheelEvent(QWheelEvent *event)
{
    event->ignore();
    if (!m_forwardingEnabled || !m_view || m_view == this || m_view->window() != window() ||
        !m_view->inherits("QQuickFlickable") || !m_view->isVisible() || !m_view->isEnabled() ||
        !m_view->property("interactive").toBool() ||
        event->modifiers().testFlag(Qt::ControlModifier) ||
        qAbs(event->angleDelta().x()) > qAbs(event->angleDelta().y()) ||
        qAbs(event->pixelDelta().x()) > qAbs(event->pixelDelta().y()))
        return;
    const auto position = m_view->mapFromItem(this, event->position());
    if (m_outsideOnly && m_view->contains(position))
        return;
    // Do not reconstruct scrolling from QML's delta-only WheelEvent wrapper:
    // native phase/source/device/timestamp are needed by Flickable's gesture state.
    QWheelEvent forwarded(position, event->globalPosition(), event->pixelDelta(),
                          event->angleDelta(), event->buttons(), event->modifiers(), event->phase(),
                          event->inverted(), event->source(), event->pointingDevice());
    forwarded.setTimestamp(event->timestamp());
    forwarded.ignore();
    QCoreApplication::sendEvent(m_view, &forwarded);
    event->setAccepted(forwarded.isAccepted());
}
} // namespace EchoMusic
