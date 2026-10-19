#include "cast/SsdpDiscovery.h"
#include <QSignalSpy>
#include <QtTest>
using namespace EchoMusic;
namespace EchoMusic
{
class SsdpDiscoveryTestAccess
{
public:
    static void activate(SsdpDiscovery &discovery)
    {
        discovery.m_running = true;
    }
    static void feed(SsdpDiscovery &discovery, const QByteArray &packet,
                     const QHostAddress &sender = QHostAddress("192.168.1.50"))
    {
        discovery.accept(packet, sender);
    }
    static void expire(SsdpDiscovery &discovery, qint64 time)
    {
        discovery.expire(time);
    }
};
} // namespace EchoMusic
class CastDiscoveryTest : public QObject
{
    Q_OBJECT
    static QByteArray response(const QByteArray &usn, const QByteArray &location,
                               const QByteArray &age = "1800")
    {
        // Normative SSDP datagrams, not vendor API responses or device fixtures.
        return "HTTP/1.1 200 OK\r\nUSN: " + usn + "\r\nLOCATION: " + location +
               "\r\nST: urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
               "CACHE-CONTROL: max-age=" +
               age + "\r\n\r\n";
    }
private slots:
    void identitiesMergeWhileEndpointChangesInvalidateTheProbe()
    {
        SsdpDiscovery discovery;
        SsdpDiscoveryTestAccess::activate(discovery);
        QSignalSpy changed(&discovery, &SsdpDiscovery::candidateChanged);
        const auto id = QByteArray("uuid:device-1::urn:schemas-upnp-org:device:MediaRenderer:1");
        SsdpDiscoveryTestAccess::feed(discovery, response(id, "http://0.0.0.0:8000/device.xml"));
        QCOMPARE(discovery.candidates().size(), 1);
        QCOMPARE(discovery.candidates().first().identity, QString("uuid:device-1"));
        QCOMPARE(discovery.candidates().first().location,
                 QString("http://192.168.1.50:8000/device.xml"));
        QCOMPARE(changed.size(), 1);
        SsdpDiscoveryTestAccess::feed(discovery,
                                      response("uuid:device-1::upnp:rootdevice",
                                               "http://192.168.1.50:8000/device.xml", "7200"));
        QCOMPARE(discovery.candidates().size(), 1);
        QCOMPARE(changed.size(), 1);
        QVERIFY(discovery.candidates().first().expiresAt > 7000000);
        SsdpDiscoveryTestAccess::feed(discovery,
                                      response(id, "http://192.168.1.51:8000/device.xml"));
        QCOMPARE(changed.size(), 2);
        QCOMPARE(discovery.candidates().size(), 1);
    }
    void rejectsUntrustedEndpointsAndBoundsCandidateLifetime()
    {
        SsdpDiscovery discovery;
        SsdpDiscoveryTestAccess::activate(discovery);
        for (const auto &url :
             {"http://127.0.0.1/private", "http://localhost/private",
              "http://user:password@192.168.1.50/device.xml", "file:///tmp/device.xml",
              "https://192.168.1.50/device.xml#fragment"})
            SsdpDiscoveryTestAccess::feed(discovery, response("uuid:reject", url));
        QCOMPARE(discovery.candidates().size(), 0);
        for (int i = 0; i < 100; ++i)
            SsdpDiscoveryTestAccess::feed(
                discovery, response("uuid:device-" + QByteArray::number(i),
                                    "http://192.168.1.50/device.xml", "99999999999999999999"));
        QCOMPARE(discovery.candidates().size(), 64);
        QSignalSpy removed(&discovery, &SsdpDiscovery::candidateRemoved);
        const auto expiry = discovery.candidates().first().expiresAt;
        SsdpDiscoveryTestAccess::expire(discovery, expiry + 1);
        QCOMPARE(discovery.candidates().size(), 0);
        QCOMPARE(removed.size(), 64);
    }
    void notifyAndStoppedDiscoveryCannotReviveStaleDevices()
    {
        SsdpDiscovery discovery;
        SsdpDiscoveryTestAccess::activate(discovery);
        const auto alive =
            QByteArray("NOTIFY * HTTP/1.1\r\nUSN: uuid:device-1\r\nNTS: ssdp:alive\r\n"
                       "NT: upnp:rootdevice\r\nLOCATION: http://192.168.1.50/device.xml\r\n\r\n");
        SsdpDiscoveryTestAccess::feed(discovery, alive);
        QCOMPARE(discovery.candidates().size(), 1);
        QSignalSpy removed(&discovery, &SsdpDiscovery::candidateRemoved);
        SsdpDiscoveryTestAccess::feed(discovery, "NOTIFY * HTTP/1.1\r\nUSN: uuid:device-1\r\n"
                                                 "NTS: ssdp:byebye\r\n\r\n");
        QCOMPARE(discovery.candidates().size(), 0);
        QCOMPARE(removed.size(), 1);
        SsdpDiscoveryTestAccess::feed(discovery, alive);
        discovery.stop();
        QVERIFY(!discovery.running());
        QCOMPARE(discovery.candidates().size(), 0);
        QCOMPARE(removed.size(), 2);
        SsdpDiscoveryTestAccess::feed(discovery, alive);
        QCOMPARE(discovery.candidates().size(), 0);
    }
};
QTEST_GUILESS_MAIN(CastDiscoveryTest)
#include "CastDiscoveryTest.moc"
