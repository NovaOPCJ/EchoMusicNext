#include "NetworkTransport.h"
#include "NetworkBootstrap.h"
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QFile>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
using namespace EchoMusic;
class NetworkVideoTest : public QObject
{
    Q_OBJECT
private slots:
    void multimediaUsesManagedRangeStreamWithoutAnAudioOutput_data()
    {
        QTest::addColumn<bool>("authenticated");
        QTest::newRow("anonymous-proxy") << false;
        QTest::newRow("explicit-basic-proxy") << true;
    }
    void multimediaUsesManagedRangeStreamWithoutAnAudioOutput()
    {
        QFETCH(bool, authenticated);
        QFile file(QFINDTESTDATA("../fixtures/network/silent-video-test.mp4"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto media = file.readAll();
        QTcpServer proxy;
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        int requests = 0;
        bool usedRange = false;
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            auto *peer = proxy.nextPendingConnection();
            auto bytes = std::make_shared<QByteArray>();
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer, bytes] {
                *bytes += peer->readAll();
                if (!bytes->contains("\r\n\r\n"))
                    return;
                if (authenticated && !bytes->toLower().contains("proxy-authorization:")) {
                    peer->write("HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: "
                                "Basic realm=\"video\"\r\nContent-Length: 0\r\nProxy-Connection: "
                                "keep-alive\r\n\r\n");
                    bytes->clear();
                    return;
                }
                if (authenticated)
                    QVERIFY(bytes->toLower().contains(
                        "proxy-authorization: basic " +
                        QByteArray("proxy-user:proxy-secret").toBase64().toLower()));
                ++requests;
                QVERIFY(bytes->startsWith("GET http://video-source.invalid/video.mp4 HTTP/1.1") ||
                        bytes->startsWith("HEAD http://video-source.invalid/video.mp4 HTTP/1.1"));
                const auto lower = bytes->toLower();
                usedRange |= lower.contains("range: bytes=");
                qint64 offset = 0;
                const auto range = lower.indexOf("range: bytes=");
                if (range >= 0)
                    offset = lower.mid(range + 13).split('-').first().toLongLong();
                if (offset < 0 || offset >= media.size()) {
                    peer->write("HTTP/1.1 416 Range Not Satisfiable\r\nContent-Length: "
                                "0\r\nContent-Range: bytes */" +
                                QByteArray::number(media.size()) + "\r\nConnection: close\r\n\r\n");
                    peer->disconnectFromHost();
                    return;
                }
                const auto part = media.mid(offset);
                QByteArray header =
                    range >= 0 ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
                header += "Content-Type: video/mp4\r\nAccept-Ranges: bytes\r\nContent-Length: " +
                          QByteArray::number(part.size()) + "\r\n";
                if (range >= 0)
                    header += "Content-Range: bytes " + QByteArray::number(offset) + '-' +
                              QByteArray::number(media.size() - 1) + '/' +
                              QByteArray::number(media.size()) + "\r\n";
                peer->write(header + "Connection: close\r\n\r\n" +
                            (bytes->startsWith("HEAD ") ? QByteArray() : part));
                peer->disconnectFromHost();
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("http://127.0.0.1:%1").arg(proxy.serverPort());
        if (authenticated)
            config.username = "proxy-user";
        transport.apply(config, authenticated ? QString("proxy-secret") : QString());
        const auto resource =
            transport.registerMedia(QUrl("http://video-source.invalid/video.mp4"));
        QMediaPlayer player;
        QVideoSink sink;
        int validFrames = 0;
        connect(&sink, &QVideoSink::videoFrameChanged, &sink, [&](const QVideoFrame &frame) {
            if (frame.isValid())
                ++validFrames;
        });
        player.setVideoSink(&sink);
        QVERIFY(!player.audioOutput());
        player.setSource(resource);
        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(validFrames >= 2 || player.error() != QMediaPlayer::NoError,
                                 10000);
        QVERIFY2(player.error() == QMediaPlayer::NoError, qPrintable(player.errorString()));
        QVERIFY(validFrames >= 2);
        QVERIFY(requests > 0);
        QVERIFY(usedRange);
        QVERIFY(player.duration() > 0);
        player.stop();
        transport.releaseMedia(resource);
    }
};
int main(int argc, char **argv)
{
    // Hostile process proxies must not steal the private local resource URL.
    qputenv("http_proxy", "http://127.0.0.1:1");
    qputenv("no_proxy", "");
    initializeNetworkEnvironment();
    QGuiApplication app(argc, argv);
    NetworkVideoTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "NetworkVideoTest.moc"
