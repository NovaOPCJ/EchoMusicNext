#pragma once
#include <QObject>
#include <QTimer>
#include <functional>

namespace EchoMusic
{
/// GUI-thread notifications only; resolving the new OS policy remains on the
/// transport workers. Coalesce native bursts without polling or querying DNS.
class SystemNetworkMonitor final : public QObject
{
    Q_OBJECT
public:
    explicit SystemNetworkMonitor(QObject *parent = nullptr);
signals:
    /// Proxy-only notifications affect System mode; topology/DNS changes also
    /// invalidate explicit PAC and failed-route caches in the other modes.
    void changed(bool proxyOnly);

private:
    void schedule(bool proxyOnly);
    QTimer m_debounce;
    bool m_proxyOnly = true;
};
/// The returned backend and its native subscriptions belong to parent. Calls
/// notify on the parent's thread and unregisters before destruction completes.
QObject *createSystemNetworkMonitor(QObject *parent, std::function<void(bool)> notify);
} // namespace EchoMusic
