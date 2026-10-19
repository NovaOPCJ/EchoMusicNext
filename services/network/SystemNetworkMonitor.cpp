#include "SystemNetworkMonitor.h"
#include <QNetworkInformation>
#include <utility>

namespace EchoMusic
{
SystemNetworkMonitor::SystemNetworkMonitor(QObject *parent) : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(200);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        const bool proxyOnly = std::exchange(m_proxyOnly, true);
        emit changed(proxyOnly);
    });
    createSystemNetworkMonitor(this, [this](bool proxyOnly) { schedule(proxyOnly); });
    QNetworkInformation::loadDefaultBackend();
    if (auto *information = QNetworkInformation::instance()) {
        // Backends may publish their first known state asynchronously. Learning
        // that startup state is not a topology change: closing connections here
        // used to cancel the application's very first requests.
        connect(information, &QNetworkInformation::reachabilityChanged, this,
                [this, previous = information->reachability()](
                    QNetworkInformation::Reachability value) mutable {
                    const bool wasKnown = previous != QNetworkInformation::Reachability::Unknown;
                    const bool changed = previous != value;
                    previous = value;
                    if (wasKnown && changed)
                        schedule(false);
                });
        connect(information, &QNetworkInformation::transportMediumChanged, this,
                [this, previous = information->transportMedium()](
                    QNetworkInformation::TransportMedium value) mutable {
                    const bool wasKnown = previous != QNetworkInformation::TransportMedium::Unknown;
                    const bool changed = previous != value;
                    previous = value;
                    if (wasKnown && changed)
                        schedule(false);
                });
    }
}
void SystemNetworkMonitor::schedule(bool proxyOnly)
{
    m_proxyOnly = m_proxyOnly && proxyOnly;
    // Bound a continuous burst to one notification per interval instead of
    // indefinitely postponing recovery while a VPN changes several services.
    if (!m_debounce.isActive())
        m_debounce.start();
}
} // namespace EchoMusic
