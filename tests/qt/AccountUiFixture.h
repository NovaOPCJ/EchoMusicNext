#pragma once
#include "AccountController.h"
#include "StorageService.h"
#include "LibraryController.h"
namespace EchoMusic::Test
{
/// Real offline services for production Main composition. No credential import,
/// synthetic product accounts or background network initialization.
struct AccountUiFixture
{
    AccountService account;
    StorageService storage;
    SavedAccountModel saved;
    QrLoginService qr;
    UserPlaylistModel playlists;
    PlaylistMutationService mutations;
    LibraryController library;
    AccountController controller;
    explicit AccountUiFixture(ApiClient &api)
        : account(api), saved(api, account, storage), qr(api), playlists(api), mutations(api),
          library(api, playlists, mutations), controller(api, account, saved, qr, playlists, false)
    {
    }
};
} // namespace EchoMusic::Test
