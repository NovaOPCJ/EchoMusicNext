#pragma once
#include "BarrageController.h"
#include "CommentsController.h"
#include "PlaybackService.h"
namespace EchoMusic
{
/// Application-owned request/draft state; the playback page owns only demand-created UI.
class PlaybackDiscussion final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by DetailsService")
    Q_PROPERTY(bool barrageAvailable READ barrageAvailable NOTIFY changed)
    Q_PROPERTY(EchoMusic::CommentsController *comments READ comments CONSTANT)
    Q_PROPERTY(EchoMusic::BarrageController *barrage READ barrage CONSTANT)
public:
    PlaybackDiscussion(ApiClient &, PlaybackService &, bool network, QObject *,
                       CommentAuthorService *);
    bool barrageAvailable() const
    {
        return !m_playback.currentTrack().hash.isEmpty();
    }
    CommentsController *comments()
    {
        return &m_comments;
    }
    BarrageController *barrage()
    {
        return &m_barrage;
    }
    Q_INVOKABLE void openComments();
    Q_INVOKABLE void closeComments();
    Q_INVOKABLE void setPresented(bool);
    void shutdown();
signals:
    void changed();
    void commentsRequested();
    void notice(const QString &, bool warning);

private:
    PlaybackService &m_playback;
    CommentsController m_comments;
    BarrageController m_barrage;
    bool m_presented = false, m_commentsOpen = false, m_closed = false;
};
} // namespace EchoMusic
