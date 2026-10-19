#pragma once
#include "ApiClient.h"
#include <QJsonArray>
#include <QPointer>
namespace EchoMusic
{
class PlaybackService;
class PlaybackQueueModel;
/// Playback-owned DiscoverFlow replenishment. Page browsing and refresh never
/// own the lifetime of an active recommendation queue.
class DiscoverQueueController final : public QObject
{
    Q_OBJECT
public:
    DiscoverQueueController(PlaybackService &, ApiClient &);
    ~DiscoverQueueController() override;
    static QString queueKey()
    {
        return QStringLiteral("queue:home-discover");
    }
    static QJsonArray songs(const QJsonObject &domain);
    bool replenish();
    bool busy() const
    {
        return m_request != 0;
    }
    void advance(bool automatic, bool preserveFailureChain = false);
    void cancelWait()
    {
        m_waiting = false;
    }
    void shutdown();
signals:
    void completed(bool added);
    void notice(const QString &message);

private:
    friend class DiscoverQueueControllerTestAccess;
    void changed();
    void request();
    void succeeded(quint64 id, const QByteArray &bytes);
    void finish(bool added);
    void cancel();
    PlaybackService &m_playback;
    ApiClient &m_api;
    quint64 m_request = 0, m_session = 0, m_revision = 0, m_seenRevision = 0, m_waitSequence = 0;
    QPointer<PlaybackQueueModel> m_queue;
    QString m_seenTrack;
    int m_attempt = 0;
    bool m_waiting = false, m_automatic = false, m_preserveFailure = false, m_closed = false,
         m_merging = false;
};
} // namespace EchoMusic
