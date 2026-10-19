#include "EchoPopupDismissGuard.h"
#include <QMouseEvent>
#include <QTimer>
namespace EchoMusic
{
EchoPopupDismissGuard::~EchoPopupDismissGuard()
{
    if (m_window)
        m_window->removeEventFilter(this);
}
void EchoPopupDismissGuard::setOverlay(QQuickItem *item)
{
    if (m_overlay == item)
        return;
    disconnect(m_windowChanged);
    m_overlay = item;
    ++m_generation;
    if (item)
        m_windowChanged =
            connect(item, &QQuickItem::windowChanged, this, &EchoPopupDismissGuard::attachWindow);
    attachWindow();
    emit changed();
}
void EchoPopupDismissGuard::setDimmer(QQuickItem *item)
{
    if (m_dimmer == item)
        return;
    m_dimmer = item;
    ++m_generation;
    emit changed();
}
void EchoPopupDismissGuard::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    ++m_generation;
    emit changed();
}
void EchoPopupDismissGuard::attachWindow()
{
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = m_overlay ? m_overlay->window() : nullptr;
    ++m_generation;
    if (m_window)
        m_window->installEventFilter(this);
}
bool EchoPopupDismissGuard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window || !m_enabled || !m_overlay || !m_dimmer ||
        event->type() != QEvent::MouseButtonPress)
        return false;
    const auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton)
        return false;
    const auto point = m_overlay->mapFromScene(mouse->position());
    if (m_overlay->childAt(point.x(), point.y()) != m_dimmer)
        return false;
    const auto generation = m_generation;
    // Do not open a confirmation in the middle of delivering this press: the
    // new popup could consume the same outside event and immediately close.
    QTimer::singleShot(0, this, [this, generation, point] {
        if (generation == m_generation && m_enabled && m_overlay && m_dimmer &&
            m_dimmer->isVisible() && m_overlay->childAt(point.x(), point.y()) == m_dimmer)
            emit outsidePressed();
    });
    return false; // Qt still owns event delivery and modal input blocking.
}
} // namespace EchoMusic
