#pragma once
#include "AccountService.h"
#include "ProfileService.h"
#include "SavedAccountModel.h"
#include "QrLoginService.h"
#include "VerificationService.h"
#include "LoginCandidateModel.h"
#include <QtQml/qqmlregistration.h>

namespace EchoMusic
{
struct PreferenceStrengthModelRegistration
{
    Q_GADGET
    QML_FOREIGN(PreferenceStrengthModel)
    QML_NAMED_ELEMENT(PreferenceStrengthModel)
    QML_UNCREATABLE("Preference presentation model")
};
struct ListeningPreferencesRegistration
{
    Q_GADGET
    QML_FOREIGN(ListeningPreferencesService)
    QML_NAMED_ELEMENT(ListeningPreferencesService)
    QML_UNCREATABLE("Application-owned account preferences")
};
struct ContentBlacklistRegistration
{
    Q_GADGET
    QML_FOREIGN(ContentBlacklistService)
    QML_NAMED_ELEMENT(ContentBlacklistService)
    QML_UNCREATABLE("Application-owned recommendation blacklist")
};
struct ContentBlacklistModelRegistration
{
    Q_GADGET
    QML_FOREIGN(ContentBlacklistModel)
    QML_NAMED_ELEMENT(ContentBlacklistModel)
    QML_UNCREATABLE("Account-scoped blacklist entries")
};
struct ProfileServiceRegistration
{
    Q_GADGET
    QML_FOREIGN(ProfileService)
    QML_NAMED_ELEMENT(ProfileService)
    QML_UNCREATABLE("Application-owned profile")
};
struct SocialUserModelRegistration
{
    Q_GADGET
    QML_FOREIGN(SocialUserModel)
    QML_NAMED_ELEMENT(SocialUserModel)
    QML_UNCREATABLE("Application-owned social list")
};
struct LoginDeviceModelRegistration
{
    Q_GADGET
    QML_FOREIGN(LoginDeviceModel)
    QML_NAMED_ELEMENT(LoginDeviceModel)
    QML_UNCREATABLE("Application-owned login devices")
};
// Keep these wrappers in the UI module. Services do not depend on QML; only
// application-owned presentation APIs, never credential storage, are exported.
struct AccountServiceRegistration
{
    Q_GADGET
    QML_FOREIGN(AccountService)
    QML_NAMED_ELEMENT(AccountService)
    QML_UNCREATABLE("Application-owned account")
};
struct SavedAccountRegistration
{
    Q_GADGET
    QML_FOREIGN(SavedAccountModel)
    QML_NAMED_ELEMENT(SavedAccountModel)
    QML_UNCREATABLE("Application-owned account catalog")
};
struct QrLoginRegistration
{
    Q_GADGET
    QML_FOREIGN(QrLoginService)
    QML_NAMED_ELEMENT(QrLoginService)
    QML_UNCREATABLE("Application-owned login session")
};
struct LoginCandidateRegistration
{
    Q_GADGET
    QML_FOREIGN(LoginCandidateModel)
    QML_NAMED_ELEMENT(LoginCandidateModel)
    QML_UNCREATABLE("Application-owned SMS candidates")
};
struct VerificationRegistration
{
    Q_GADGET
    QML_FOREIGN(VerificationService)
    QML_NAMED_ELEMENT(VerificationService)
    QML_UNCREATABLE("Application-owned shared verification")
};
} // namespace EchoMusic
