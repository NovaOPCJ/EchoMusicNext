#include "ProfileRouteCoordinator.h"
#include "AccountController.h"
#include "ProfileService.h"
#include "RouteController.h"
namespace EchoMusic
{
ProfileRouteCoordinator::ProfileRouteCoordinator(AccountController &account,
                                                 ProfileService &profile, RouteController &router,
                                                 bool online, QObject *parent)
    : QObject(parent)
{
    router.registerPage(
        {"profile",
         "/main/profile",
         QUrl("qrc:/qt/qml/EchoMusic/Profile/ProfilePage.qml"),
         {},
         true,
         true,
         false,
         {{"service", QVariant::fromValue(&account)}, {"profile", QVariant::fromValue(&profile)}}});
    connect(&router, &RouteController::pageActivated, this,
            [this, &account, online](RouteContext *page) {
                if (page->name() != "profile" || !online || page->instanceId() == m_instance)
                    return;
                m_instance = page->instanceId();
                account.account()->reloadProfile();
                account.account()->loadGrade();
            });
}
} // namespace EchoMusic
