#pragma once
#include <QObject>
#include <QVariantMap>
#include <memory>
#ifdef Q_OS_LINUX
#include <QDBusMessage>
#endif

namespace EchoMusic
{
/// GUI-thread owner of a system-sleep lease, independent of window visibility.
/// Prevents idle system suspend during playback, never forces the display on.
/// System suspend drops the lease; resume reconciles the current playback intent.
class SystemSleepInhibitor final : public QObject
{
    Q_OBJECT
public:
    explicit SystemSleepInhibitor(QObject *parent = nullptr);
    ~SystemSleepInhibitor() override;
    void setRequired(bool required);
    bool active() const
    {
        return m_active;
    }
    QString error() const
    {
        return m_error;
    }
public slots:
    void setSystemSuspended(bool suspended);
signals:
    void changed();
    void systemResumed();
private slots:
#ifdef Q_OS_LINUX
    void portalResponse(uint response, const QVariantMap &results, const QDBusMessage &message);
#else
    void portalResponse(uint response, const QVariantMap &results);
#endif
private:
    struct Native;
    std::unique_ptr<Native> m_native;
    bool m_required = false, m_suspended = false, m_active = false;
    QString m_error;
    void reconcile();
    void publish(bool active, const QString &error = {});
};
} // namespace EchoMusic
