#include "SystemSleepInhibitor.h"
#include <QThread>
namespace EchoMusic
{
void SystemSleepInhibitor::setRequired(bool required)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_required == required)
        return;
    m_required = required;
    reconcile();
}
void SystemSleepInhibitor::setSystemSuspended(bool suspended)
{
    if (m_suspended == suspended)
        return;
    m_suspended = suspended;
    reconcile();
    if (!suspended)
        emit systemResumed();
}
void SystemSleepInhibitor::publish(bool active, const QString &error)
{
    if (m_active == active && m_error == error)
        return;
    m_active = active;
    m_error = error;
    emit changed();
}
#ifndef Q_OS_LINUX
void SystemSleepInhibitor::portalResponse(uint, const QVariantMap &) {}
#endif
} // namespace EchoMusic
