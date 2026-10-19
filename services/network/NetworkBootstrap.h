#pragma once
namespace EchoMusic
{
/// Before Qt/backend worker creation only: initialize libcurl globals. Qt Multimedia's FFmpeg HTTP
/// reader honors process proxy variables independently of Qt: private loopback media URLs must
/// bypass that second route and enter the application's own bridge. Preserve both existing no_proxy
/// variants; all origin decisions remain owned by NetworkPreferences (including explicit manual
/// <-loopback> rules).
void initializeNetworkEnvironment();
} // namespace EchoMusic
