#pragma once
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

namespace EchoMusic
{
/// A header region or page ancestor forwards the complete native wheel event.
/// Children keep Qt's normal delivery priority; the view owns all scrolling.
class EchoWheelForwarder : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *wheelView READ wheelView WRITE setWheelView NOTIFY wheelViewChanged)
    Q_PROPERTY(bool forwardOutsideViewOnly READ forwardOutsideViewOnly WRITE
                   setForwardOutsideViewOnly NOTIFY forwardOutsideViewOnlyChanged)
    Q_PROPERTY(bool forwardingEnabled READ forwardingEnabled WRITE setForwardingEnabled NOTIFY
                   forwardingEnabledChanged)
public:
    explicit EchoWheelForwarder(QQuickItem *parent = nullptr) : QQuickItem(parent) {}
    QQuickItem *wheelView() const
    {
        return m_view;
    }
    void setWheelView(QQuickItem *view);
    bool forwardOutsideViewOnly() const
    {
        return m_outsideOnly;
    }
    void setForwardOutsideViewOnly(bool value);
    bool forwardingEnabled() const
    {
        return m_forwardingEnabled;
    }
    void setForwardingEnabled(bool value);
signals:
    void wheelViewChanged();
    void forwardOutsideViewOnlyChanged();
    void forwardingEnabledChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    QPointer<QQuickItem> m_view;
    bool m_outsideOnly = false;
    bool m_forwardingEnabled = true;
};
} // namespace EchoMusic
