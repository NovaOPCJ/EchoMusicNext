#include "SongShareText.h"
#include "ShareTarget.h"
namespace EchoMusic
{
bool isSongShareHash(const QString &hash)
{
    const auto trimmed = hash.trimmed();
    if (trimmed.size() != 32)
        return false;
    for (const auto &c : trimmed) {
        const auto n = c.unicode();
        if (!((n >= '0' && n <= '9') || (n >= 'a' && n <= 'f') || (n >= 'A' && n <= 'F')))
            return false;
    }
    return true;
}
namespace
{
ShareTarget songTarget(const PlaybackTrack &track)
{
    ShareTarget target{ShareResource::Song, track.hash.trimmed().toLower(), {}};
    if (track.albumId)
        target.query.insert("albumId", QString::number(track.albumId));
    const auto songId = track.songId ? track.songId : track.id.toULongLong();
    if (songId)
        target.query.insert("albumAudioId", QString::number(songId));
    return target;
}
} // namespace
QString songShareUrl(const PlaybackTrack &track)
{
    return shareWebUrl(songTarget(track)).toString(QUrl::FullyEncoded);
}
QString songShareText(const PlaybackTrack &track, const QString &sharer)
{
    return shareText(songTarget(track), track.title, sharer);
}
} // namespace EchoMusic
