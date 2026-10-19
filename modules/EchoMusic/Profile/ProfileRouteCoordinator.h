#pragma once
#include <QObject>
#include <QString>
namespace EchoMusic
{
class AccountController;
class RouteController;
class ProfileService;
/// Registers the account-scoped page and starts the reference profile read flow.
/// Credentials, profile state and request revisions stay in AccountService.
class ProfileRouteCoordinator final : public QObject
{
public:
    ProfileRouteCoordinator(AccountController &account, ProfileService &profile,
                            RouteController &router, bool online = true, QObject *parent = nullptr);

private:
    QString m_instance;
};
} // namespace EchoMusic
