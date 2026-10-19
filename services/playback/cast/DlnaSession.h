#pragma once
#include "NativeBridge.h"
#include "PlaybackTrack.h"
#include "NetworkPreferences.h"
#include <QElapsedTimer>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <functional>
namespace EchoMusic
{
/// AVTransport/RenderingControl protocol state, never a second playback queue.
/// Every native completion and media lease carries this session/source epoch.
class DlnaSession final : public QObject
{
    Q_OBJECT
public:
    explicit DlnaSession(NativeBridge &native, QObject *parent = nullptr);
    ~DlnaSession() override;
    void setNetwork(NetworkPreferences *network);
    void connectDevice(const QJsonObject &device);
    bool active() const
    {
        return m_active;
    }
    bool loading() const
    {
        return m_loading;
    }
    void load(const QString &source, const PlaybackTrack &track, quint64 sequence);
    void play();
    void pause();
    void stop();
    void seek(qint64 position);
    void volume(double value);
    void disconnectDevice(std::function<void()> complete);
    void shutdown(std::function<void()> complete);
    static qint64 parseTime(const QString &value);
    static QString formatTime(qint64 milliseconds);
    static bool naturalEnd(const QString &state, const QString &expectedUri,
                           const QString &reportedUri, qint64 position, qint64 duration,
                           bool pendingCommand);
signals:
    void connected(int volume);
    void outputEvent(const QJsonObject &event);
    void failed(const QString &message);
    void takenOver();
    void sourceExpired();
    void volumeSettled(int value);

private:
    using Completion = std::function<void(bool, QJsonObject)>;
    using Arguments = QList<QPair<QString, QString>>;
    void action(const QString &service, const QString &name, const Arguments &arguments,
                Completion complete);
    void cancelObservation();
    void cancelPending();
    void releaseResource();
    void publish(QJsonObject event);
    void observe();
    void setPlaying(bool playing);
    void flushVolume();
    NativeBridge &m_native;
    QPointer<NetworkPreferences> m_network;
    QMetaObject::Connection m_rejection;
    bool m_sourceExpired = false;
    struct Pending
    {
        quint64 revision;
        QString action;
        Completion complete;
    };
    QHash<quint64, Pending> m_pending;
    QString m_location, m_av, m_rc, m_cm, m_uri;
    QUrl m_resource;
    QTimer m_poll, m_volumeTimer;
    QElapsedTimer m_commandTime;
    quint64 m_revision = 1, m_sequence = 0;
    qint64 m_position = 0, m_duration = 0;
    int m_offline = 0, m_desiredVolume = -1, m_confirmedVolume = -1;
    bool m_volumeInFlight = false;
    bool m_active = false, m_loading = false, m_polling = false, m_playing = false,
         m_seenPlaying = false, m_ended = false, m_takenOver = false, m_closed = false;
};
} // namespace EchoMusic
