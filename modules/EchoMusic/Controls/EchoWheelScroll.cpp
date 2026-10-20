#include "EchoWheelScroll.h"
#include <QGuiApplication>
#include <QAccessibilityHints>
#include <QStyleHints>
#include <QWheelEvent>
#include <QQuickWindow>
#include <QSignalBlocker>
#include <cmath>

namespace EchoMusic
{
EchoWheelScrollAttached::EchoWheelScrollAttached(QObject *parent) : QObject(parent)
{
    auto *item = qobject_cast<QQuickItem *>(parent);
    if (!item || !item->inherits("QQuickFlickable"))
        return;
    m_view = item;
    item->installEventFilter(this);
    connect(item, &QQuickItem::windowChanged, this, &EchoWheelScrollAttached::observeWindow);
    observeWindow(item->window());
    connect(item, SIGNAL(contentYChanged()), this, SLOT(contentPositionChanged()));
    connect(item, SIGNAL(interactiveChanged()), this, SLOT(cancel()));
    connect(item, &QQuickItem::visibleChanged, this, &EchoWheelScrollAttached::cancel);
    connect(item, &QQuickItem::enabledChanged, this, &EchoWheelScrollAttached::cancel);
    connect(item, &QQuickItem::heightChanged, this, &EchoWheelScrollAttached::cancel);
    m_animation.setDuration(120);
    m_animation.setEasingCurve(QEasingCurve::OutCubic);
    auto *accessibility = QGuiApplication::styleHints()->accessibility();
    m_reducedMotion = accessibility->motionPreference() == Qt::MotionPreference::ReducedMotion;
    connect(accessibility, &QAccessibilityHints::motionPreferenceChanged, this,
            &EchoWheelScrollAttached::updateMotionPreference);
    connect(&m_animation, &QVariantAnimation::stateChanged, this,
            [this] { emit runningChanged(); });
    connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        if (!acceptsScroll()) {
            cancel();
            return;
        }
        writePosition(value.toReal());
    });
}
void EchoWheelScrollAttached::writePosition(qreal position)
{
    if (!m_view)
        return;
    // Virtual views can revise estimated extents while delegates enter.
    const qreal origin = m_view->property("originY").toReal();
    const qreal first = origin - m_view->property("topMargin").toReal();
    const qreal last =
        qMax(first, origin + m_view->property("contentHeight").toReal() - m_view->height() +
                        m_view->property("bottomMargin").toReal());
    const QPointer<EchoWheelScrollAttached> guard(this);
    m_writing = true;
    m_view->setProperty("contentY", qBound(first, position, last));
    if (guard)
        m_writing = false;
}
void EchoWheelScrollAttached::observeWindow(QQuickWindow *window)
{
    cancel();
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = window;
    if (m_window)
        m_window->installEventFilter(this);
}
void EchoWheelScrollAttached::cancel()
{
    m_animation.stop();
}
void EchoWheelScrollAttached::contentPositionChanged()
{
    if (m_writing)
        return;
    // Flickable::setContentHeight() runs fixupY even when still in bounds and
    // rounds a fractional position. That native correction is not a user's
    // new scroll command: preserve the outstanding destination across it.
    if (running() && m_view) {
        const qreal current = m_view->property("contentY").toReal();
        const qreal expected = m_animation.currentValue().toReal();
        if (qFuzzyCompare(current + 1, expected + 1) ||
            qFuzzyCompare(current + 1, std::round(expected) + 1))
            return;
    }
    cancel();
}
void EchoWheelScrollAttached::setMotionEnabled(bool enabled)
{
    if (m_motionEnabled == enabled)
        return;
    m_motionEnabled = enabled;
    settleWithoutAnimation();
    emit motionEnabledChanged();
}
void EchoWheelScrollAttached::updateMotionPreference(Qt::MotionPreference preference)
{
    m_reducedMotion = preference == Qt::MotionPreference::ReducedMotion;
    settleWithoutAnimation();
}
void EchoWheelScrollAttached::settleWithoutAnimation()
{
    if (!running() || (m_motionEnabled && !m_reducedMotion))
        return;
    const auto target = m_target;
    cancel();
    if (acceptsScroll())
        writePosition(target);
}
void EchoWheelScrollAttached::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!enabled)
        cancel();
    emit enabledChanged();
}
bool EchoWheelScrollAttached::acceptsScroll() const
{
    return m_enabled && m_view && m_window && m_window->isVisible() &&
           m_window->visibility() != QWindow::Minimized && m_view->isVisible() &&
           m_view->isEnabled() && m_view->property("interactive").toBool();
}
bool EchoWheelScrollAttached::scrollBy(qreal distance)
{
    if (!acceptsScroll() || !std::isfinite(distance) || distance == 0)
        return false;
    const qreal current = m_view->property("contentY").toReal();
    const qreal origin = m_view->property("originY").toReal();
    const qreal first = origin - m_view->property("topMargin").toReal();
    const qreal last =
        qMax(first, origin + m_view->property("contentHeight").toReal() - m_view->height() +
                        m_view->property("bottomMargin").toReal());
    const bool accumulating =
        m_animation.state() == QAbstractAnimation::Running && (m_target - current) * distance > 0;
    // New same-direction ticks add to the outstanding destination, rather than
    // discarding the previous tick's untravelled distance. Reversal starts now.
    const qreal target = qBound(first, (accumulating ? m_target : current) + distance, last);
    m_animation.stop();
    if (qFuzzyCompare(target + 1, current + 1))
        return false;
    QMetaObject::invokeMethod(m_view, "cancelFlick");
    m_target = target;
    if (!m_motionEnabled || m_reducedMotion) {
        writePosition(target);
        return true;
    }
    {
        // Reconfiguring a stopped QVariantAnimation can emit valueChanged at
        // its previous progress. Do not expose those intermediate positions.
        const QSignalBlocker blocker(&m_animation);
        m_animation.setStartValue(current);
        m_animation.setEndValue(target);
        m_animation.setCurrentTime(0);
    }
    m_animation.start();
    return true;
}
bool EchoWheelScrollAttached::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Hide || event->type() == QEvent::WindowStateChange ||
        event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin)
        cancel();
    if (object != m_view)
        return false;
    if (event->type() != QEvent::Wheel)
        return false;
    auto *wheel = static_cast<QWheelEvent *>(event);
    // Preserve real touchpad pixel deltas, phases and momentum in Flickable.
    if (!wheel->pixelDelta().isNull() || wheel->phase() != Qt::NoScrollPhase) {
        cancel();
        return false;
    }
    if (wheel->modifiers().testFlag(Qt::ControlModifier)) {
        cancel();
        wheel->ignore();
        return true;
    }
    if (!m_enabled || qAbs(wheel->angleDelta().x()) > qAbs(wheel->angleDelta().y()) ||
        wheel->angleDelta().y() == 0)
        return false;
    const qreal distance =
        -wheel->angleDelta().y() / 120.0 * QGuiApplication::styleHints()->wheelScrollLines() * 24;
    wheel->setAccepted(scrollBy(distance));
    return true;
}
} // namespace EchoMusic
