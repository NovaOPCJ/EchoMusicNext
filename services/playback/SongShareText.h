#pragma once
#include "PlaybackTrack.h"
#include <QString>
namespace EchoMusic
{
/// The shared typed share contract owns URLs and receiving validation.
bool isSongShareHash(const QString &hash);
/// Source-format web landing link retaining album/catalog audio identity.
QString songShareUrl(const PlaybackTrack &track);
/// 生成昵称、歌曲标题和链接组成的分享文本。昵称缺失时使用 EchoMusic；
/// 不可分享的歌曲返回空串。
QString songShareText(const PlaybackTrack &track, const QString &sharer);
} // namespace EchoMusic
