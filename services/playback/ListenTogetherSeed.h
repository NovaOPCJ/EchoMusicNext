#pragma once
#include "PlaybackTrack.h"

namespace EchoMusic
{
/// EchoMusic's creation window is chosen before transmitting audio references.
/// Keep repeated queue entries: they participate in the source window positions.
inline PlaybackTracks listenTogetherSeed(const PlaybackTracks &queue, const PlaybackTrack &current)
{
    const auto input = queue.isEmpty() ? PlaybackTracks{current} : queue;
    PlaybackTracks songs;
    for (auto track : input) {
        if (track.hash.trimmed().isEmpty() || !track.songId)
            continue;
        track.hash = track.hash.trimmed();
        songs.append(track);
    }
    if (songs.size() <= 50)
        return songs;
    int currentIndex = -1;
    for (int i = 0; i < songs.size(); ++i) {
        if (songs[i].hash.compare(current.hash, Qt::CaseInsensitive) == 0) {
            currentIndex = i;
            break;
        }
    }
    const int start = currentIndex < 5                  ? 0
                      : currentIndex + 5 > songs.size() ? int(songs.size()) - 10
                                                        : currentIndex - 5;
    return songs.mid(start, 10);
}
} // namespace EchoMusic
