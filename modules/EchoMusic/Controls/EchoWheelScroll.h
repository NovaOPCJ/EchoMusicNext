#pragma once
#include <QPointer>
#include <QQuickItem>
#include <QVariantAnimation>
#include <QtQml/qqmlregistration.h>

namespace EchoMusic
{
// One controller per actual Flickable, including reparented page/popup bars.
// Qt still routes input to nested controls first and owns pixel/gesture input.
class EchoWheelScrollAttached final : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
public:
    explicit EchoWheelScrollAttached(QObject *parent);
    bool enabled() const
    {
        return m_enabled;
    }
    bool running() const
    {
        return m_animation.state() == QAbstractAnimation::Running;
    }
    void setEnabled(bool enabled);
    Q_INVOKABLE bool scrollBy(qreal distance);
signals:
    void enabledChanged();
    void runningChanged();
private slots:
    void cancel();

private:
    bool eventFilter(QObject *object, QEvent *event) override;
    void observeWindow(QQuickWindow *window);
    QPointer<QQuickItem> m_view;
    QPointer<QQuickWindow> m_window;
    QVariantAnimation m_animation;
    qreal m_target = 0;
    bool m_enabled = true;
    bool m_writing = false;
};
class EchoWheelScroll final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use the Flickable's attached wheel controller")
    QML_ATTACHED(EchoWheelScrollAttached)
public:
    static EchoWheelScrollAttached *qmlAttachedProperties(QObject *object)
    {
        return new EchoWheelScrollAttached(object);
    }
};
} // namespace EchoMusic
QML_DECLARE_TYPEINFO(EchoMusic::EchoWheelScroll, QML_HAS_ATTACHED_PROPERTIES)
