#include "AboutService.h"
#include "SettingsMarkdownRenderer.h"
#include "NetworkBootstrap.h"
#include <QGuiApplication>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextDocument>
#include <QTextBlock>
#include <QtTest>
using namespace EchoMusic;
class AboutServiceTest : public QObject
{
    Q_OBJECT
private slots:
    void atomContractAndOriginValidation()
    {
        // Contract tests based on the sampled Atom schema. These entries do not
        // claim that EchoMusicNext has published a release (its real feed is empty).
        const QByteArray entry =
            R"(<feed xmlns="http://www.w3.org/2005/Atom"><entry><link rel="alternate" href="https://github.com/hoowhoami/EchoMusicNext/releases/tag/v0.2.0-beta.1"/><content type="html">&lt;p&gt;Release notes&lt;/p&gt;</content></entry></feed>)";
        QString error;
        const auto parsed = AboutService::parseFeed(entry, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(parsed.size(), 1);
        QCOMPARE(parsed.first().version, QString("0.2.0-beta.1"));
        QCOMPARE(parsed.first().notes, QString("Release notes"));
        auto foreign = entry;
        foreign.replace("github.com", "example.com");
        QVERIFY(AboutService::parseFeed(foreign, &error).isEmpty());
        QVERIFY(error.isEmpty());
        AboutService::parseFeed("<!DOCTYPE feed [<!ENTITY x 'test'>]><feed/>", &error);
        QVERIFY(!error.isEmpty());
        AboutService::parseFeed("<feed", &error);
        QVERIFY(!error.isEmpty());
    }
    void checksAreSingleFlightAndChannelChangeCancelsOldResult()
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("preferences.ini"), QSettings::IniFormat);
        ApiClient api(DeviceIdentity::temporary());
        NetworkPreferences network(settings, api);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        network.setGithubProxyUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));
        int requests = 0;
        bool hold = false;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto socket = server.nextPendingConnection();
            socket->setParent(&server);
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                auto bytes = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", bytes);
                if (!bytes.endsWith("\r\n\r\n") || socket->property("handled").toBool())
                    return;
                socket->setProperty("handled", true);
                ++requests;
                if (hold)
                    return;
                const QByteArray body = "<feed xmlns=\"http://www.w3.org/2005/Atom\"/>";
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) +
                    "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        AboutService about(settings, network);
        QVERIFY(about.changelog().contains("v0.1.0"));
        const auto localNotes = about.changelog();
        about.check();
        about.check();
        QTRY_VERIFY(!about.checking());
        QCOMPARE(requests, 1);
        QVERIFY(about.resultOpen());
        QCOMPARE(about.resultState(), AboutService::NoPublishedRelease);
        QCOMPARE(about.changelog(), localNotes);
        QVERIFY(about.statusText().contains(QStringLiteral("还没有已发布")));
        hold = true;
        about.check();
        QTRY_COMPARE(requests, 2);
        about.setPrerelease(true);
        QVERIFY(!about.checking());
        QVERIFY(about.statusText().isEmpty());
        about.shutdown();
        api.shutdown();
    }
    void nativeMarkdownDoesNotLoadImagesOrUnsafeLinks()
    {
        SettingsMarkdownRenderer renderer;
        QFont font;
        font.setFamilies({"Helvetica Neue", "PingFang SC", "sans-serif"});
        font.setPixelSize(14);
        const auto html = renderer.render(
            "# 标题 Heading\n\n说明 "
            "**Bold**\n\n[发布页](https://github.com/hoowhoami/EchoMusicNext)\n\n"
            "[本地](file:///private/secret) ![图片](https://example.invalid/pixel.png)",
            font, Qt::black, Qt::gray, Qt::blue);
        QTextDocument document;
        document.setHtml(html);
        QVERIFY(document.toPlainText().contains("标题 Heading"));
        QVERIFY(document.toPlainText().contains("说明 Bold"));
        int links = 0;
        for (auto block = document.begin(); block.isValid(); block = block.next())
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const auto format = it.fragment().charFormat();
                QVERIFY(!format.isImageFormat());
                if (format.isAnchor()) {
                    ++links;
                    QCOMPARE(QUrl(format.anchorHref()).scheme(), QString("https"));
                }
            }
        QCOMPARE(links, 1);
    }
};
int main(int argc, char **argv)
{
    initializeNetworkEnvironment();
    QGuiApplication app(argc, argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    AboutServiceTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "AboutServiceTest.moc"
