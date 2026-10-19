#include "StartupIntegration.h"
#include <QCoreApplication>
#import <ServiceManagement/ServiceManagement.h>
namespace EchoMusic
{
bool macLoginItemEnabled()
{
    return SMAppService.mainAppService.status == SMAppServiceStatusEnabled;
}
QString setMacLoginItemEnabled(bool enabled)
{
    @autoreleasepool {
        auto *service = SMAppService.mainAppService;
        if (enabled && service.status == SMAppServiceStatusEnabled)
            return {};
        if (!enabled && service.status == SMAppServiceStatusNotRegistered)
            return {};
        NSError *error = nil;
        const BOOL accepted = enabled ? [service registerAndReturnError:&error]
                                      : [service unregisterAndReturnError:&error];
        if (!accepted)
            return QCoreApplication::translate("StartupIntegration", "系统自启动设置失败：%1")
                .arg(QString::fromNSString(error.localizedDescription));
        if (enabled && service.status == SMAppServiceStatusRequiresApproval)
            return QCoreApplication::translate("StartupIntegration",
                                               "请在系统设置的登录项中允许 EchoMusicNext");
        if (enabled && service.status != SMAppServiceStatusEnabled)
            return QCoreApplication::translate("StartupIntegration",
                                               "系统尚未启用自启动，请检查登录项设置");
        return {};
    }
}
} // namespace EchoMusic
