#include "PlaybackDiscussion.h"
namespace EchoMusic
{
PlaybackDiscussion::PlaybackDiscussion(ApiClient &api, PlaybackService &playback, bool network,
                                       QObject *parent, CommentAuthorService *authors)
    : QObject(parent), m_playback(playback), m_comments(api, network, this, authors),
      m_barrage(api, nullptr, network, this, true)
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    connect(&m_comments, &CommentsController::readFailed, this,
            [this](const QString &message) { emit notice(message, true); });
    connect(&m_comments, &CommentsController::notice, this, &PlaybackDiscussion::notice);
    connect(&m_barrage, &BarrageController::notice, this, &PlaybackDiscussion::notice);
    connect(&playback, &PlaybackService::changed, this, [this] {
        const auto &track = m_playback.currentTrack();
        m_barrage.setResource(track.hash, track.title);
        emit changed();
    });
    connect(&api, &ApiClient::sessionChanged, this, [this] {
        if (m_presented && m_commentsOpen)
            m_comments.start();
    });
    const auto &track = playback.currentTrack();
    m_barrage.setResource(track.hash, track.title);
}
void PlaybackDiscussion::openComments()
{
    if (m_closed || !m_presented || !m_playback.hasTrack())
        return;
    // EchoMusic snapshots the opening track; automatic next-song must not
    // silently retarget an existing reply draft or an open comment thread.
    const auto track = m_playback.currentTrack();
    m_comments.clear();
    m_comments.configure(ApiClient::CommentResource::Music,
                         track.songId ? QString::number(track.songId) : track.id, track.title,
                         track.songId);
    m_commentsOpen = true;
    m_comments.setPresented(true);
    emit commentsRequested();
}
void PlaybackDiscussion::closeComments()
{
    m_commentsOpen = false;
    m_comments.setPresented(false);
}
void PlaybackDiscussion::setPresented(bool value)
{
    if (m_closed || m_presented == value)
        return;
    m_presented = value;
    m_barrage.setPresented(value);
    m_comments.setPresented(value && m_commentsOpen);
}
void PlaybackDiscussion::shutdown()
{
    if (m_closed)
        return;
    m_closed = true;
    m_comments.shutdown();
    m_barrage.shutdown();
    disconnect(&m_playback, nullptr, this, nullptr);
}
} // namespace EchoMusic
