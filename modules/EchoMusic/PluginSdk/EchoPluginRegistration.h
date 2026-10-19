#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <functional>
namespace EchoMusic
{
/// A revision-specific lease. Replacing a contribution disposes its previous
/// lease before publishing the new one. dispose is always idempotent.
class EchoPluginRegistration final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Returned by PluginSdk registration methods")
    Q_PROPERTY(QString key READ key CONSTANT)
    Q_PROPERTY(QString revision READ revision CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
public:
    EchoPluginRegistration(QString key, quint64 revision, QObject *parent);
    QString key() const
    {
        return m_key;
    }
    QString revision() const
    {
        return QString::number(m_revision);
    }
    bool active() const
    {
        return m_active;
    }
    void stage(std::function<bool()> publish, std::function<void()> remove);
    bool commit();
    Q_INVOKABLE void dispose();
signals:
    void activeChanged();

private:
    QString m_key;
    quint64 m_revision;
    bool m_active = true, m_published = false;
    std::function<bool()> m_publish;
    std::function<void()> m_remove;
};
} // namespace EchoMusic
