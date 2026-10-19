#include "SystemSleepInhibitor.h"
#include <QCoreApplication>
#include <QAbstractNativeEventFilter>
#include <windows.h>
namespace EchoMusic
{
struct SystemSleepInhibitor::Native final : QAbstractNativeEventFilter
{
    SystemSleepInhibitor &owner;
    explicit Native(SystemSleepInhibitor &owner) : owner(owner)
    {
        qApp->installNativeEventFilter(this);
    }
    ~Native() override
    {
        qApp->removeNativeEventFilter(this);
        SetThreadExecutionState(ES_CONTINUOUS);
    }
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *) override
    {
        const auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_POWERBROADCAST) {
            if (msg->wParam == PBT_APMSUSPEND)
                owner.setSystemSuspended(true);
            else if (msg->wParam == PBT_APMRESUMEAUTOMATIC || msg->wParam == PBT_APMRESUMESUSPEND)
                owner.setSystemSuspended(false);
        }
        return false;
    }
};
SystemSleepInhibitor::SystemSleepInhibitor(QObject *parent)
    : QObject(parent), m_native(std::make_unique<Native>(*this))
{
}
SystemSleepInhibitor::~SystemSleepInhibitor() = default;
void SystemSleepInhibitor::reconcile()
{
    const auto required = m_required && !m_suspended;
    // This API's lease belongs to the calling thread: acquire and clear on the
    // same Qt GUI thread. ES_DISPLAY_REQUIRED/away mode deliberately omitted.
    if (!SetThreadExecutionState(ES_CONTINUOUS | (required ? ES_SYSTEM_REQUIRED : 0)))
        publish(false, tr("未能调整系统休眠策略（%1）").arg(GetLastError()));
    else
        publish(required);
}
} // namespace EchoMusic
