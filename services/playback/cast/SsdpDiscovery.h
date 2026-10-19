#pragma once
#include <QObject>
#include <QHostAddress>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>
#include <QUdpSocket>

namespace EchoMusic
{
struct SsdpCandidate
{
    QString identity, location, serviceType, server;
    QHostAddress sender;
    qint64 firstSeen = 0, lastSeen = 0, expiresAt = 0;
};
/// Qt owns discovery sockets and the monotonic candidate cache. Candidates are
/// not playable targets until the native UPnP description confirms AVTransport.
/// start/stop is gated by the output service's browsing/active-session policy.
class SsdpDiscovery final : public QObject
{
    Q_OBJECT
public:
    explicit SsdpDiscovery(QObject *parent = nullptr);
    void start();
    void stop();
    void refresh();
    QList<SsdpCandidate> candidates() const;
    bool running() const
    {
        return m_running;
    }
signals:
    void candidateChanged(const EchoMusic::SsdpCandidate &candidate);
    void candidateRemoved(const QString &identity);
    void searchStarted();
    void searchFinished();
    void errorOccurred(const QString &message);

private:
    friend class SsdpDiscoveryTestAccess;
    void receive(QUdpSocket &socket);
    void accept(const QByteArray &packet, const QHostAddress &sender);
    void expire(qint64 now);
    void sendTarget();
    QUdpSocket m_searchSocket, m_notifySocket;
    QElapsedTimer m_clock;
    QTimer m_searchTimer, m_expiryTimer;
    QHash<QString, SsdpCandidate> m_candidates;
    bool m_running = false;
    quint64 m_generation = 0;
    int m_round = 0, m_target = 0;
};
} // namespace EchoMusic
Q_DECLARE_METATYPE(EchoMusic::SsdpCandidate)
