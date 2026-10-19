#include "SystemNetworkMonitor.h"
#include <QLoggingCategory>
#include <SystemConfiguration/SystemConfiguration.h>

namespace EchoMusic
{
namespace
{
class MacSystemNetworkMonitor final : public QObject
{
public:
    MacSystemNetworkMonitor(QObject *parent, std::function<void(bool)> notify)
        : QObject(parent), m_notify(std::move(notify))
    {
        SCDynamicStoreContext context{0, this, nullptr, nullptr, nullptr};
        m_store = SCDynamicStoreCreate(nullptr, CFSTR("EchoMusicNext network"), changed, &context);
        if (!m_store)
            return;
        const void *keys[] = {
            CFSTR("State:/Network/Global/Proxies"), CFSTR("State:/Network/Global/DNS"),
            CFSTR("State:/Network/Global/IPv4"), CFSTR("State:/Network/Global/IPv6")};
        const void *patterns[] = {CFSTR("State:/Network/Service/.*/Proxies"),
                                  CFSTR("Setup:/Network/Service/.*/Proxies"),
                                  CFSTR("State:/Network/Service/.*/DNS")};
        auto keyArray = CFArrayCreate(nullptr, keys, 4, &kCFTypeArrayCallBacks);
        auto patternArray = CFArrayCreate(nullptr, patterns, 3, &kCFTypeArrayCallBacks);
        const bool registered = SCDynamicStoreSetNotificationKeys(m_store, keyArray, patternArray);
        CFRelease(keyArray);
        CFRelease(patternArray);
        if (registered)
            m_source = SCDynamicStoreCreateRunLoopSource(nullptr, m_store, 0);
        if (m_source)
            CFRunLoopAddSource(CFRunLoopGetMain(), m_source, kCFRunLoopCommonModes);
        else
            qWarning("Unable to subscribe to macOS network configuration changes");
    }
    ~MacSystemNetworkMonitor() override
    {
        if (m_source) {
            CFRunLoopSourceInvalidate(m_source);
            CFRelease(m_source);
        }
        if (m_store)
            CFRelease(m_store);
    }

private:
    static void changed(SCDynamicStoreRef, CFArrayRef keys, void *context)
    {
        bool proxyOnly = true;
        for (CFIndex i = 0; i < CFArrayGetCount(keys); ++i) {
            auto key = static_cast<CFStringRef>(CFArrayGetValueAtIndex(keys, i));
            proxyOnly = proxyOnly && CFStringHasSuffix(key, CFSTR("/Proxies"));
        }
        static_cast<MacSystemNetworkMonitor *>(context)->m_notify(proxyOnly);
    }
    std::function<void(bool)> m_notify;
    SCDynamicStoreRef m_store = nullptr;
    CFRunLoopSourceRef m_source = nullptr;
};
} // namespace
QObject *createSystemNetworkMonitor(QObject *parent, std::function<void(bool)> notify)
{
    return new MacSystemNetworkMonitor(parent, std::move(notify));
}
} // namespace EchoMusic
