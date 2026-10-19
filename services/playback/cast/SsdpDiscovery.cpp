#include "SsdpDiscovery.h"
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>

namespace EchoMusic
{
namespace
{
const QHostAddress group(QStringLiteral("239.255.255.250"));
const QList<QByteArray> targets{"ssdp:all", "urn:schemas-upnp-org:device:MediaRenderer:1",
                                "urn:schemas-upnp-org:device:MediaRenderer:2",
                                "urn:schemas-upnp-org:service:AVTransport:1"};
QList<QNetworkInterface> interfaces()
{
    QList<QNetworkInterface> result;
    for (const auto &network : QNetworkInterface::allInterfaces()) {
        if (!network.flags().testFlag(QNetworkInterface::IsUp) ||
            network.flags().testFlag(QNetworkInterface::IsLoopBack) ||
            !network.flags().testFlag(QNetworkInterface::CanMulticast))
            continue;
        for (const auto &entry : network.addressEntries())
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                result.append(network);
                break;
            }
    }
    return result;
}
void join(QUdpSocket &socket)
{
    const auto networks = interfaces();
    if (networks.isEmpty())
        socket.joinMulticastGroup(group);
    else
        for (const auto &network : networks)
            socket.joinMulticastGroup(group, network);
}
QString identity(const QByteArray &usn)
{
    // Vendor UUIDs need not be hexadecimal. Service suffixes belong to SSDP,
    // not the canonical device key, and may be longer than the UUID itself.
    const auto key = QString::fromLatin1(usn).trimmed().toLower().section("::", 0, 0);
    static const QRegularExpression pattern("^uuid:[a-z0-9._:-]{1,64}$");
    return pattern.match(key).hasMatch() ? key : QString();
}
QString location(const QByteArray &raw, const QHostAddress &sender)
{
    if (raw.size() > 4096 || raw.contains('\r') || raw.contains('\n'))
        return {};
    QUrl url(QString::fromUtf8(raw), QUrl::StrictMode);
    if (!url.isValid() || (url.scheme() != "http" && url.scheme() != "https") ||
        url.host().isEmpty() || !url.userInfo().isEmpty() || !url.fragment().isEmpty())
        return {};
    const QHostAddress host(url.host());
    if (url.host() == "0.0.0.0") {
        if (sender.protocol() != QAbstractSocket::IPv4Protocol || sender.isNull() ||
            sender == QHostAddress(QHostAddress::AnyIPv4))
            return {};
        url.setHost(sender.toString());
    } else if ((url.host().compare("localhost", Qt::CaseInsensitive) == 0 || host.isLoopback()) &&
               !sender.isLoopback())
        return {};
    return url.toString(QUrl::FullyEncoded);
}
} // namespace
SsdpDiscovery::SsdpDiscovery(QObject *parent) : QObject(parent)
{
    m_clock.start();
    m_searchTimer.setSingleShot(true);
    m_expiryTimer.setInterval(15000);
    connect(&m_searchSocket, &QUdpSocket::readyRead, this, [this] { receive(m_searchSocket); });
    connect(&m_notifySocket, &QUdpSocket::readyRead, this, [this] { receive(m_notifySocket); });
    connect(&m_searchTimer, &QTimer::timeout, this, &SsdpDiscovery::sendTarget);
    connect(&m_expiryTimer, &QTimer::timeout, this, [this] { expire(m_clock.elapsed()); });
}
void SsdpDiscovery::start()
{
    if (m_running)
        return;
    if (!m_searchSocket.bind(QHostAddress::AnyIPv4, 0,
                             QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit errorOccurred(tr("无法搜索投放设备"));
        return;
    }
    m_running = true;
    m_searchSocket.setSocketOption(QAbstractSocket::MulticastTtlOption, 2);
    join(m_searchSocket);
    // Port 1900 may be unavailable while active search still works.
    // Keep discovery usable rather than discarding reachable devices.
    if (m_notifySocket.bind(QHostAddress::AnyIPv4, 1900,
                            QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        join(m_notifySocket);
    m_expiryTimer.start();
    refresh();
}
void SsdpDiscovery::stop()
{
    ++m_generation;
    m_running = false;
    m_searchTimer.stop();
    m_expiryTimer.stop();
    m_searchSocket.close();
    m_notifySocket.close();
    const auto keys = m_candidates.keys();
    m_candidates.clear();
    for (const auto &key : keys)
        emit candidateRemoved(key);
    emit searchFinished();
}
void SsdpDiscovery::refresh()
{
    if (!m_running)
        return;
    ++m_generation;
    m_round = m_target = 0;
    m_searchTimer.stop();
    expire(m_clock.elapsed());
    emit searchStarted();
    sendTarget();
}
void SsdpDiscovery::sendTarget()
{
    if (!m_running)
        return;
    const auto packet = QByteArray("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n"
                                   "MAN: \"ssdp:discover\"\r\nMX: 2\r\nST: ") +
                        targets.at(m_target++) + "\r\nUSER-AGENT: EchoMusic/1.0 UPnP/1.1\r\n\r\n";
    const auto networks = interfaces();
    if (networks.isEmpty())
        m_searchSocket.writeDatagram(packet, group, 1900);
    else
        for (const auto &network : networks) {
            m_searchSocket.setMulticastInterface(network);
            m_searchSocket.writeDatagram(packet, group, 1900);
        }
    if (m_target < targets.size()) {
        m_searchTimer.start(120);
    } else if (++m_round < 3) {
        m_target = 0;
        m_searchTimer.start(m_round == 1 ? 1800 : 3600);
    } else {
        // MX=2 permits the final responses to arrive after the last send.
        QTimer::singleShot(2000, this, [this, generation = m_generation] {
            if (m_running && m_generation == generation && m_round == 3)
                emit searchFinished();
        });
    }
}
void SsdpDiscovery::receive(QUdpSocket &socket)
{
    // Bound one event-loop batch as well as datagram/cache sizes. No flood can
    // monopolize input handling; Qt delivers readyRead for remaining datagrams.
    for (int count = 0; count < 64 && socket.hasPendingDatagrams(); ++count) {
        const auto datagram = socket.receiveDatagram(8193);
        if (m_running && datagram.data().size() <= 8192)
            accept(datagram.data(), datagram.senderAddress());
    }
    if (socket.hasPendingDatagrams())
        QMetaObject::invokeMethod(this, [this, &socket] { receive(socket); }, Qt::QueuedConnection);
}
void SsdpDiscovery::accept(const QByteArray &packet, const QHostAddress &sender)
{
    if (!m_running || packet.size() > 8192)
        return;
    const auto end = packet.indexOf("\r\n\r\n");
    if (end < 0)
        return;
    const auto lines = packet.first(end).split('\n');
    if (lines.isEmpty())
        return;
    const auto first = lines.first().trimmed();
    const bool response = first.startsWith("HTTP/1.1 200") || first.startsWith("HTTP/1.0 200");
    if (!response && !first.startsWith("NOTIFY "))
        return;
    QHash<QByteArray, QByteArray> headers;
    for (int i = 1; i < lines.size(); ++i) {
        const auto line = lines.at(i).trimmed();
        const auto colon = line.indexOf(':');
        if (colon > 0)
            headers.insert(line.first(colon).toLower(), line.sliced(colon + 1).trimmed());
    }
    const auto key = identity(headers.value("usn"));
    if (key.isEmpty())
        return;
    if (!response) {
        const auto type = headers.value("nts").toLower();
        if (type == "ssdp:byebye") {
            if (m_candidates.remove(key))
                emit candidateRemoved(key);
            return;
        }
        if (type != "ssdp:alive")
            return;
    }
    const auto endpoint = location(headers.value("location"), sender);
    if (endpoint.isEmpty())
        return;
    const auto now = m_clock.elapsed();
    expire(now);
    if (!m_candidates.contains(key) && m_candidates.size() >= 64)
        return;
    static const QRegularExpression ttlPattern("max-age\\s*=\\s*(\\d+)",
                                               QRegularExpression::CaseInsensitiveOption);
    const auto match = ttlPattern.match(QString::fromLatin1(headers.value("cache-control")));
    bool ok = false;
    const auto seconds = match.hasMatch() ? match.captured(1).toULongLong(&ok) : 0;
    const auto ttl = ok && seconds > 0 ? qMin(quint64(86400), seconds) : quint64(1800);
    auto candidate = m_candidates.value(key);
    const bool changed = candidate.identity.isEmpty() || candidate.location != endpoint;
    if (candidate.identity.isEmpty()) {
        candidate.identity = key;
        candidate.firstSeen = now;
    }
    candidate.location = endpoint;
    candidate.sender = sender;
    candidate.lastSeen = now;
    candidate.expiresAt = now + qint64(ttl) * 1000;
    candidate.serviceType = QString::fromLatin1(headers.value("st", headers.value("nt")));
    candidate.server = QString::fromLatin1(headers.value("server"));
    m_candidates.insert(key, candidate);
    if (changed)
        emit candidateChanged(candidate);
}
void SsdpDiscovery::expire(qint64 now)
{
    const auto keys = m_candidates.keys();
    for (const auto &key : keys)
        if (m_candidates.value(key).expiresAt <= now) {
            m_candidates.remove(key);
            emit candidateRemoved(key);
        }
}
QList<SsdpCandidate> SsdpDiscovery::candidates() const
{
    auto result = m_candidates.values();
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.firstSeen == b.firstSeen ? a.identity < b.identity : a.firstSeen < b.firstSeen;
    });
    return result;
}
} // namespace EchoMusic
