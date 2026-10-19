#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include "SystemNetworkMonitor.h"
#include <QWinEventNotifier>
#include <QLoggingCategory>

namespace EchoMusic
{
namespace
{
class InterfaceNetworkMonitor final : public QObject
{
public:
    InterfaceNetworkMonitor(QObject *parent, std::function<void(bool)> notify)
        : QObject(parent), m_notify(std::move(notify))
    {
        if (NotifyIpInterfaceChange(AF_UNSPEC, changed, this, FALSE, &m_handle) != NO_ERROR)
            qWarning("Unable to subscribe to Windows IP interface changes");
    }
    ~InterfaceNetworkMonitor() override
    {
        // OS callbacks never wait for the GUI. Deregister/join on the owner
        // thread, not from the native callback, which would deadlock on itself.
        if (m_handle)
            CancelMibChangeNotify2(m_handle);
    }

private:
    static void CALLBACK changed(void *context, MIB_IPINTERFACE_ROW *, MIB_NOTIFICATION_TYPE type)
    {
        if (type == MibInitialNotification)
            return;
        auto *self = static_cast<InterfaceNetworkMonitor *>(context);
        QMetaObject::invokeMethod(self, [self] { self->m_notify(false); }, Qt::QueuedConnection);
    }
    HANDLE m_handle = nullptr;
    std::function<void(bool)> m_notify;
};
class RegistryNetworkMonitor final : public QObject
{
public:
    RegistryNetworkMonitor(HKEY root, const wchar_t *path, QObject *parent,
                           std::function<void(bool)> notify)
        : QObject(parent), m_notify(std::move(notify))
    {
        if (RegOpenKeyExW(root, path, 0, KEY_NOTIFY, &m_key) != ERROR_SUCCESS)
            return;
        m_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!m_event)
            return;
        m_notifier = new QWinEventNotifier(m_event, this);
        connect(m_notifier, &QWinEventNotifier::activated, this, [this] {
            m_notifier->setEnabled(false);
            ResetEvent(m_event);
            // Re-arm before delivery: changes during coalescing aren't lost.
            arm();
            m_notify(true);
        });
        arm();
    }
    ~RegistryNetworkMonitor() override
    {
        // Closing a watched key signals its event. Stop Qt delivery first.
        delete m_notifier;
        if (m_key)
            RegCloseKey(m_key);
        if (m_event)
            CloseHandle(m_event);
    }

private:
    void arm()
    {
        const auto result = RegNotifyChangeKeyValue(
            m_key, TRUE,
            REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_THREAD_AGNOSTIC,
            m_event, TRUE);
        m_notifier->setEnabled(result == ERROR_SUCCESS);
        if (result != ERROR_SUCCESS)
            qWarning("Unable to subscribe to Windows proxy configuration changes");
    }
    HKEY m_key = nullptr;
    HANDLE m_event = nullptr;
    QWinEventNotifier *m_notifier = nullptr;
    std::function<void(bool)> m_notify;
};
} // namespace
QObject *createSystemNetworkMonitor(QObject *parent, std::function<void(bool)> notify)
{
    auto *owner = new QObject(parent);
    new InterfaceNetworkMonitor(owner, notify);
    constexpr auto path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
    new RegistryNetworkMonitor(HKEY_CURRENT_USER, path, owner, notify);
    new RegistryNetworkMonitor(HKEY_LOCAL_MACHINE, path, owner, std::move(notify));
    return owner;
}
} // namespace EchoMusic
