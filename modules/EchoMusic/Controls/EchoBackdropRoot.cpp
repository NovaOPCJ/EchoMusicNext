#include "EchoBackdropRoot.h"
#include <QDynamicPropertyChangeEvent>
#include <QQuickWindow>
#include <QVariant>
namespace EchoMusic
{
namespace
{
constexpr auto rootKey = "echoBackdropRoot";
constexpr auto scopesKey = "echoBackdropScopes";
void updateScope(QQuickWindow *window, QQuickItem *scope, bool active)
{
    auto scopes = window->property(scopesKey).toList();
    scopes.removeIf([scope](const QVariant &value) {
        auto *item = value.value<QQuickItem *>();
        return !item || item == scope;
    });
    if (active)
        scopes.append(QVariant::fromValue(scope));
    window->setProperty(scopesKey, scopes);
    auto *source = scopes.isEmpty() ? nullptr : scopes.constLast().value<QQuickItem *>();
    if (window->property(rootKey).value<QQuickItem *>() != source)
        window->setProperty(rootKey, QVariant::fromValue(source));
}
} // namespace
EchoBackdropRoot::EchoBackdropRoot(QQuickItem *parent) : QQuickItem(parent)
{
    connect(this, &QQuickItem::windowChanged, this, &EchoBackdropRoot::registerWindow);
    registerWindow(window());
}
EchoBackdropRoot::~EchoBackdropRoot()
{
    // QQuickItem's base teardown emits windowChanged after this derived type
    // has gone. A typed slot must be disconnected while its receiver still lives.
    disconnect(this, &QQuickItem::windowChanged, this, &EchoBackdropRoot::registerWindow);
    registerWindow(nullptr);
}
void EchoBackdropRoot::registerWindow(QQuickWindow *window)
{
    if (m_window && m_window != window)
        updateScope(m_window, this, false);
    m_window = window;
    if (window)
        updateScope(window, this, m_samplingActive);
}
void EchoBackdropRoot::setSamplingActive(bool active)
{
    if (active == m_samplingActive)
        return;
    m_samplingActive = active;
    registerWindow(window());
    emit samplingActiveChanged();
}
EchoBackdropSampler::EchoBackdropSampler(QObject *parent) : QObject(parent) {}
void EchoBackdropSampler::setSurface(QQuickItem *surface)
{
    if (surface == m_surface)
        return;
    m_surface = surface;
    observe();
    emit surfaceChanged();
}
void EchoBackdropSampler::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    observe();
}
void EchoBackdropSampler::observe()
{
    for (const auto &connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
    if (m_window)
        m_window->removeEventFilter(this);
    m_window = m_surface ? m_surface->window() : nullptr;
    if (m_window)
        m_window->installEventFilter(this);
    if (m_surface) {
        m_connections.append(
            connect(m_surface, &QQuickItem::windowChanged, this, &EchoBackdropSampler::observe));
        m_connections.append(
            connect(m_surface, &QObject::destroyed, this, &EchoBackdropSampler::observe));
    }
    if (m_enabled) {
        for (auto *item = m_surface.data(); item; item = item->parentItem()) {
            m_connections.append(
                connect(item, &QQuickItem::parentChanged, this, &EchoBackdropSampler::observe));
            m_connections.append(
                connect(item, &QQuickItem::xChanged, this, &EchoBackdropSampler::changed));
            m_connections.append(
                connect(item, &QQuickItem::yChanged, this, &EchoBackdropSampler::changed));
            m_connections.append(
                connect(item, &QQuickItem::widthChanged, this, &EchoBackdropSampler::changed));
            m_connections.append(
                connect(item, &QQuickItem::heightChanged, this, &EchoBackdropSampler::changed));
            m_connections.append(
                connect(item, &QQuickItem::scaleChanged, this, &EchoBackdropSampler::changed));
            m_connections.append(
                connect(item, &QQuickItem::rotationChanged, this, &EchoBackdropSampler::changed));
        }
    }
    emit changed();
}
bool EchoBackdropSampler::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && event->type() == QEvent::DynamicPropertyChange &&
        static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == rootKey)
        emit changed();
    return false;
}
QQuickItem *EchoBackdropSampler::source() const
{
    return m_enabled && m_window ? m_window->property(rootKey).value<QQuickItem *>() : nullptr;
}
void EchoBackdropSampler::setPadding(qreal padding)
{
    const auto value = qBound(0.0, padding, 256.0);
    if (qFuzzyCompare(m_padding, value))
        return;
    m_padding = value;
    emit changed();
}
QRectF EchoBackdropSampler::sourceRect() const
{
    auto *root = source();
    // Include the entire blur kernel beyond the rounded panel. Cropping to the
    // panel itself produces clamped streaks at its edges.
    return root && m_surface
               ? m_surface->mapRectToItem(root, QRectF(-m_padding, -m_padding,
                                                       m_surface->width() + 2 * m_padding,
                                                       m_surface->height() + 2 * m_padding))
               : QRectF();
}
} // namespace EchoMusic
