#include "ListenTogetherController.h"
#include "PlaybackService.h"
#include <QDateTime>
#include <QScopeGuard>
#include <cmath>
namespace EchoMusic
{
namespace
{
bool sameRoomAudio(const PlaybackTrack &current, const PlaybackTrack &song,
                   const QJsonObject &facts)
{
    if (current.id != song.id || current.hash.isEmpty())
        return false;
    return current.hash.compare(song.hash, Qt::CaseInsensitive) == 0 ||
           current.hash.compare(facts.value("original_hash").toString(), Qt::CaseInsensitive) == 0;
}
} // namespace

ListenRoomAudio ListenTogetherController::trackAudio(const PlaybackTrack &track) const
{
    const auto facts = m_roomFacts.value(track.key().toLower());
    return {facts.value("original_hash").toString(track.hash),
            facts.value("original_song_id").toString(QString::number(track.songId))};
}
ListenRoomProgress ListenTogetherController::currentProgress() const
{
    return {trackAudio(m_playback.currentTrack()), m_playback.positionMs(), m_playback.playing(),
            m_playback.playMode() == PlaybackService::SingleLoop ? 2
            : m_playback.playMode() == PlaybackService::Shuffle  ? 3
                                                                 : 1};
}
bool ListenTogetherController::roomSourceAllowed(const PlaybackTrack &track) const
{
    const auto facts = m_roomFacts.value(track.key().toLower());
    const int genting = facts.value("genting_code").toInt();
    return joined() && m_playback.sourceKey() == queueKey() &&
           facts.value("can_play_code").toInt() == 1 && genting >= 2 && genting <= 6;
}
int ListenTogetherController::remoteSongIndex() const
{
    const auto hash = m_remote.value("hash").toString();
    const auto id = m_remote.value("song_id").toString();
    for (int i = 0; i < m_tracks.size(); ++i)
        if ((!hash.isEmpty() && m_tracks[i].hash.compare(hash, Qt::CaseInsensitive) == 0) ||
            (!id.isEmpty() && m_tracks[i].id == id))
            return i;
    return -1;
}
void ListenTogetherController::updateQueue(bool activate)
{
    if (m_detached && !activate)
        return;
    int index = -1;
    for (int i = 0; i < m_tracks.size(); ++i)
        if (sameRoomAudio(m_playback.currentTrack(), m_tracks[i],
                          m_roomFacts.value(m_tracks[i].key().toLower())))
            index = i;
    if (activate && index >= 0 && m_playback.sourceKey() != queueKey()) {
        const bool dispatching = std::exchange(m_applyDispatching, true);
        const auto finish = qScopeGuard([this, dispatching] {
            m_applyDispatching = dispatching;
            if (!dispatching)
                releaseRemoteApply();
        });
        m_remoteApplying = true;
        m_remoteSequence = m_playback.audioSequence();
        m_lastTrackKey = m_playback.currentTrack().key();
        m_lastPlaying = m_playback.playing();
        m_lastMode = m_playback.playMode();
        m_playback.m_sourceKey = queueKey();
        m_playback.m_queueTitle = m_activeRoom.value("name", tr("一起听")).toString();
        m_playback.queueCatalog()->activate(queueKey(), m_playback.m_queueTitle, m_tracks, index);
        emit m_playback.changed();
        return;
    }
    auto *model = m_playback.queueCatalog()->modelForKey(queueKey());
    if (m_playback.sourceKey() == queueKey() && model && model->tracks() != m_tracks)
        model->replace(m_tracks, index);
}
void ListenTogetherController::restoreQueue()
{
    const bool ownsAudio = m_playback.sourceKey() == queueKey();
    auto *catalog = m_playback.queueCatalog();
    if (ownsAudio) {
        m_remoteApplying = true;
        m_playback.stop();
        auto *previous = catalog->modelForKey(m_previousQueue);
        const auto key = previous ? m_previousQueue : QStringLiteral("default");
        if (!previous)
            previous = catalog->modelForKey(key);
        const auto tracks = previous ? previous->tracks() : PlaybackTracks{};
        const auto index = previous ? previous->currentIndex() : -1;
        m_playback.m_sourceKey = key;
        m_playback.m_queueTitle = catalog->titleForKey(key);
        catalog->activate(key, m_playback.m_queueTitle, tracks, index);
        m_playback.m_track =
            previous && previous->at(index) ? *previous->at(index) : PlaybackTrack{};
        m_playback.m_duration = qint64(m_playback.m_track.durationMs);
        m_playback.m_position = 0;
        m_playback.m_autoplay = false;
        m_playback.publishMedia();
        emit m_playback.changed();
        emit m_playback.positionChanged();
        m_remoteApplying = false;
    }
    catalog->removeInactiveRoomQueue();
    m_previousQueue.clear();
}
void ListenTogetherController::syncPlayback(bool force, std::function<void()> terminal)
{
    if (!joined()) {
        if (terminal)
            terminal();
        return;
    }
    if (m_syncRequest) {
        if (force) {
            m_forceAfterLoad = true;
            ++m_syncRevision;
        }
        after(m_syncRequest, terminal);
        return;
    }
    ListenMusicRequest request;
    request.action = ListenMusicAction::Sync;
    request.roomId = m_roomId;
    const auto id = m_syncRequest = m_api.listenTogetherMusic(request);
    const auto revision = ++m_syncRevision;
    watch(
        id, Scope::Room,
        [this, force, revision](const QJsonObject &data) {
            if (revision != m_syncRevision)
                return;
            m_remote = data;
            m_remoteReceived.restart();
            const auto version = data.value("version").toString();
            if (!version.isEmpty() && version != m_version)
                loadSongs();
            applyRemote(force);
            emit changed();
        },
        [this, revision](const QJsonObject &error) {
            if (revision == m_syncRevision)
                reportError(error, false);
        });
    after(id, [this] {
        m_syncRequest = 0;
        if (std::exchange(m_forceAfterLoad, false))
            syncPlayback(true);
    });
    after(id, terminal);
}
void ListenTogetherController::startRemoteTrack(int index, bool)
{
    if (index < 0 || index >= m_tracks.size())
        return;
    const auto key = m_tracks[index].key();
    if (m_playbackRetry.value(key, -1) > m_retryClock.elapsed())
        return;
    const bool dispatching = std::exchange(m_applyDispatching, true);
    const auto finish = qScopeGuard([this, dispatching] {
        m_applyDispatching = dispatching;
        if (!dispatching)
            releaseRemoteApply();
    });
    m_remoteApplying = true;
    m_remoteSequence = 0;
    m_seekStarted = -1;
    m_remoteSeekPending = m_remoteTarget > 1000;
    m_playback.playTracks(m_tracks, index, m_activeRoom.value("name", tr("一起听")).toString(),
                          queueKey());
    m_remoteSequence = m_playback.audioSequence();
    m_playback.m_resumePosition = m_remoteTarget > 1000 ? m_remoteTarget : -1;
    m_playback.m_autoplay = m_remote.value("pause_code").toInt() == 1 && !m_guestPaused;
    m_lastTrackKey = m_playback.currentTrack().key();
    m_lastPlaying = m_playback.playing();
    m_lastMode = m_playback.playMode();
}
void ListenTogetherController::applyRemote(bool force)
{
    if (!joined() || m_detached || m_remote.isEmpty() || m_ownerPending ||
        !m_ownerCommands.isEmpty() ||
        (!force && owner() && m_retryClock.elapsed() < m_ownerGraceUntil))
        return;
    const int index = remoteSongIndex();
    if (index < 0)
        return;
    const bool playing = m_remote.value("pause_code").toInt() == 1 && !m_guestPaused;
    qint64 target = qMax<qint64>(0, m_remote.value("position_ms").toInteger());
    if (playing && m_remoteReceived.isValid())
        target += m_remoteReceived.elapsed();
    m_remoteTarget = target;
    const bool sameTrack = sameRoomAudio(m_playback.currentTrack(), m_tracks[index],
                                         m_roomFacts.value(m_tracks[index].key().toLower()));
    if (sameTrack && (m_playback.m_loaded || m_playback.loading()) &&
        m_playback.errorMessage().isEmpty())
        updateQueue(true);
    if (m_playback.sourceKey() != queueKey() || !sameTrack ||
        (!m_playback.m_loaded && !m_playback.loading() && m_playback.errorMessage().isEmpty())) {
        startRemoteTrack(index, force);
        return;
    }
    if (m_playback.loading()) {
        m_playback.m_resumePosition = target;
        m_playback.m_autoplay = playing;
        return;
    }
    if (!m_playback.errorMessage().isEmpty()) {
        if (!m_playbackRetry.contains(m_playback.currentTrack().key()))
            m_playbackRetry.insert(m_playback.currentTrack().key(), m_retryClock.elapsed() + 30000);
        startRemoteTrack(index, force);
        return;
    }
    const bool dispatching = std::exchange(m_applyDispatching, true);
    const auto finish = qScopeGuard([this, dispatching] {
        m_applyDispatching = dispatching;
        if (!dispatching)
            releaseRemoteApply();
    });
    m_remoteApplying = true;
    m_remoteSequence = m_playback.audioSequence();
    if (!m_remote.value("play_mode_code").isNull()) {
        const int mode = m_remote.value("play_mode_code").toInt();
        if (mode >= 1 && mode <= 3)
            m_playback.setPlayMode(mode == 2   ? PlaybackService::SingleLoop
                                   : mode == 3 ? PlaybackService::Shuffle
                                               : PlaybackService::ListLoop);
    }
    if (m_playback.playing() != playing) {
        if (playing)
            m_playback.play();
        else
            m_playback.pause();
    }
    const auto tolerance = force ? 750 : playing ? 8000 : 1500;
    const auto key = m_tracks[index].key() + "/" +
                     QString::number(m_remote.value("position_ms").toInteger()) + "/" +
                     QString::number(m_remote.value("timestamp_seconds").toInteger());
    if (!m_guestPaused && target > 0 && !m_remoteSeekPending &&
        qAbs(m_playback.positionMs() - target) > tolerance &&
        (key != m_seekKey || !m_seekTimer.isValid() || m_seekTimer.elapsed() >= 2000)) {
        m_seekKey = key;
        m_seekTimer.restart();
        m_remoteSeekPending = true;
        m_remoteSequence = m_playback.audioSequence();
        m_seekStarted = m_retryClock.elapsed();
        m_playback.seek(target + (playing ? qMin<qint64>(10000, m_remoteSeekLatency) : 0));
    }
    releaseRemoteApply();
}
void ListenTogetherController::releaseRemoteApply()
{
    if (!m_remoteApplying || m_applyDispatching || !m_remoteSequence ||
        m_remoteSequence != m_playback.audioSequence() || m_playback.loading() ||
        m_remoteSeekPending)
        return;
    const bool expected = m_remote.value("pause_code").toInt() == 1 && !m_guestPaused;
    if (m_playback.playing() != expected && m_playback.errorMessage().isEmpty())
        return;
    m_remoteApplying = false;
    m_lastTrackKey = m_playback.currentTrack().key();
    m_lastPlaying = m_playback.playing();
    m_lastMode = m_playback.playMode();
}
void ListenTogetherController::resumeRoomPlayback()
{
    if (!joined())
        return;
    m_detached = m_guestPaused = false;
    syncPlayback(true);
    emit changed();
}
void ListenTogetherController::playRoomSong(const QString &id)
{
    if (!joined())
        return;
    if (!owner()) {
        resumeRoomPlayback();
        return;
    }
    for (int i = 0; i < m_tracks.size(); ++i)
        if (m_tracks[i].id == id) {
            m_detached = false;
            m_playback.playTracks(m_tracks, i, m_activeRoom.value("name").toString(), queueKey());
            return;
        }
}
void ListenTogetherController::localPlayRequested()
{
    if (!joined() || m_remoteApplying || m_playback.sourceKey() != queueKey())
        return;
    if (!owner()) {
        m_guestPaused = false;
        syncPlayback(true);
    }
}
void ListenTogetherController::localPauseRequested()
{
    if (joined() && !owner() && !m_remoteApplying && m_playback.sourceKey() == queueKey())
        m_guestPaused = true;
}
void ListenTogetherController::localSeekRequested(qint64 position)
{
    if (!joined() || m_remoteApplying || m_playback.sourceKey() != queueKey())
        return;
    if (owner())
        m_pendingLocalSeek = position;
    else
        syncPlayback(true);
}
bool ListenTogetherController::suppressAutomaticNext() const
{
    return m_playback.sourceKey() == queueKey() && (m_phase == Leaving || (joined() && !owner()));
}
void ListenTogetherController::nativeSeekCompleted()
{
    if (!joined() || m_playback.sourceKey() != queueKey())
        return;
    if (m_remoteSeekPending && m_remoteSequence == m_playback.audioSequence()) {
        m_remoteSeekPending = false;
        const auto elapsed = m_retryClock.elapsed() - m_seekStarted;
        if (m_seekStarted >= 0 && elapsed >= 250)
            m_remoteSeekLatency =
                qMin<qint64>(10000, qRound64(m_remoteSeekLatency * .7 + elapsed * .3));
        releaseRemoteApply();
        return;
    }
    if (owner() && !m_remoteApplying && m_pendingLocalSeek >= 0) {
        ListenMusicRequest request;
        request.action = ListenMusicAction::Player;
        request.command = ListenPlayerCommand::Seek;
        request.positionMs = m_playback.positionMs();
        m_pendingLocalSeek = -1;
        enqueueOwner(request);
    }
}
void ListenTogetherController::nativeTrackEnded()
{
    if (owner() && !m_detached)
        m_ownerAutomatic = true;
}
void ListenTogetherController::localTrackRequested()
{
    if (!m_remoteApplying)
        m_ownerAutomatic = false;
}
void ListenTogetherController::nativeTrackReady()
{
    const auto sequence = m_playback.audioSequence();
    const bool repeated = sequence != m_lastReadySequence && m_lastReadySequence &&
                          m_lastTrackKey == m_playback.currentTrack().key();
    m_lastReadySequence = sequence;
    if (!owner() || m_detached || m_remoteApplying || m_playback.sourceKey() != queueKey() ||
        !repeated)
        return;
    ListenMusicRequest request;
    request.action = ListenMusicAction::Switch;
    request.audio = trackAudio(m_playback.currentTrack());
    request.automatic = std::exchange(m_ownerAutomatic, false);
    enqueueOwner(request);
}
void ListenTogetherController::positionChanged()
{
    releaseRemoteApply();
}
void ListenTogetherController::playbackChanged()
{
    if (!joined())
        return;
    if (m_playback.sourceKey() != queueKey()) {
        // Only an explicit external source adoption detaches room playback.
        // Hydration can still be waiting for its first accepted playlist page.
        if (!m_lastTrackKey.isEmpty() && !m_detached) {
            m_detached = true;
            m_remoteApplying = m_remoteSeekPending = false;
            emit changed();
        }
        return;
    }
    if (m_playback.loading())
        return;
    if (!m_playback.errorMessage().isEmpty()) {
        m_remoteSeekPending = m_remoteApplying = false;
        m_playbackRetry.insert(m_playback.currentTrack().key(), m_retryClock.elapsed() + 30000);
        return;
    }
    if (m_remoteApplying) {
        releaseRemoteApply();
        return;
    }
    if (m_detached)
        return;
    if (!owner()) {
        if (remoteSongIndex() >= 0 &&
            !sameRoomAudio(m_playback.currentTrack(), m_tracks[remoteSongIndex()],
                           m_roomFacts.value(m_tracks[remoteSongIndex()].key().toLower())))
            applyRemote(true);
        return;
    }
    const auto key = m_playback.currentTrack().key();
    if (key != m_lastTrackKey) {
        m_lastTrackKey = key;
        ListenMusicRequest request;
        request.action = ListenMusicAction::Switch;
        request.audio = trackAudio(m_playback.currentTrack());
        request.automatic = std::exchange(m_ownerAutomatic, false);
        enqueueOwner(request);
    }
    if (m_playback.playing() != m_lastPlaying) {
        m_lastPlaying = m_playback.playing();
        ListenMusicRequest request;
        request.action = ListenMusicAction::Player;
        request.command = ListenPlayerCommand::Playing;
        request.playing = m_lastPlaying;
        enqueueOwner(request);
    }
    if (m_playback.playMode() != m_lastMode) {
        m_lastMode = m_playback.playMode();
        ListenMusicRequest request;
        request.action = ListenMusicAction::Player;
        request.command = ListenPlayerCommand::Mode;
        request.playMode = currentProgress().playMode;
        enqueueOwner(request);
    }
}
void ListenTogetherController::enqueueOwner(const ListenMusicRequest &request)
{
    if (!owner() || m_detached)
        return;
    if (m_syncRequest)
        ++m_syncRevision;
    m_ownerGraceUntil = m_retryClock.elapsed() + 3000;
    m_ownerCommands.append(request);
    sendNextOwnerCommand();
}
void ListenTogetherController::sendNextOwnerCommand()
{
    if (!owner() || m_ownerPending || m_ownerCommands.isEmpty())
        return;
    auto request = m_ownerCommands.takeFirst();
    request.roomId = m_roomId;
    request.version = m_version;
    m_ownerPending = true;
    const auto id = m_api.listenTogetherMusic(request);
    watch(
        id, Scope::Room,
        [this](const QJsonObject &data) {
            if (!data.value("version").toString().isEmpty())
                m_version = data.value("version").toString();
        },
        [this](const QJsonObject &error) { reportError(error); });
    after(id, [this] {
        m_ownerPending = false;
        sendNextOwnerCommand();
    });
}
} // namespace EchoMusic
