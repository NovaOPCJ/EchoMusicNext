#include "NetworkBootstrap.h"
#include <QCoreApplication>
#include "NetworkTransport.h"
#include "HttpRequestBody.h"
#include <QDir>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>
#include <QTcpServer>
#include <QtEndian>
#include <QtTest>

using namespace EchoMusic;
namespace
{
// Independent synthetic upstream parser: accept complete fixed/chunked HTTP
// requests and leave incomplete bytes untouched, including authentication probes.
bool takeRequest(QByteArray &buffer, QByteArray &header, QByteArray &body)
{
    const auto boundary = buffer.indexOf("\r\n\r\n");
    if (boundary < 0)
        return false;
    header = buffer.left(boundary + 4);
    body.clear();
    qint64 length = 0;
    bool chunked = false;
    for (const auto &line : header.split('\n')) {
        if (line.toLower().startsWith("content-length:"))
            length = line.mid(line.indexOf(':') + 1).trimmed().toLongLong();
        if (line.toLower().startsWith("transfer-encoding:"))
            chunked = line.toLower().contains("chunked");
    }
    qsizetype cursor = boundary + 4;
    if (!chunked) {
        if (buffer.size() - cursor < length)
            return false;
        body = buffer.mid(cursor, length);
        cursor += length;
    } else {
        for (;;) {
            const auto end = buffer.indexOf("\r\n", cursor);
            if (end < 0)
                return false;
            const auto size =
                buffer.mid(cursor, end - cursor).split(';').first().toLongLong(nullptr, 16);
            cursor = end + 2;
            if (!size) {
                if (buffer.mid(cursor, 2) == "\r\n")
                    cursor += 2;
                else {
                    const auto trailers = buffer.indexOf("\r\n\r\n", cursor);
                    if (trailers < 0)
                        return false;
                    header += buffer.mid(cursor, trailers - cursor + 2);
                    cursor = trailers + 4;
                }
                break;
            }
            if (buffer.size() - cursor < size + 2)
                return false;
            body += buffer.mid(cursor, size);
            cursor += size + 2;
        }
    }
    buffer.remove(0, cursor);
    return true;
}
QByteArray utf16(const QString &text)
{
    QByteArray bytes;
    for (const auto c : text) {
        bytes.append(char(c.unicode()));
        bytes.append(char(c.unicode() >> 8));
    }
    return bytes;
}
QByteArray le16(quint16 value)
{
    QByteArray bytes(2, '\0');
    qToLittleEndian(value, bytes.data());
    return bytes;
}
QByteArray le32(quint32 value)
{
    QByteArray bytes(4, '\0');
    qToLittleEndian(value, bytes.data());
    return bytes;
}
QByteArray field(const QByteArray &message, int offset)
{
    if (message.size() < offset + 8)
        return {};
    const auto size = qFromLittleEndian<quint16>(message.data() + offset);
    const auto start = qFromLittleEndian<quint32>(message.data() + offset + 4);
    if (start > quint32(message.size()) || size > message.size() - start)
        return {};
    return message.mid(start, size);
}
QByteArray ntlmChallenge()
{
    const auto domain = utf16("Domain"), server = utf16("Server");
    const auto info = le16(2) + le16(domain.size()) + domain + le16(1) + le16(server.size()) +
                      server + QByteArray(4, '\0');
    // Synthetic MS-NLMP CHALLENGE_MESSAGE: Unicode/NTLM/extended session/target info.
    return QByteArray("NTLMSSP\0", 8) + le32(2) + le16(domain.size()) + le16(domain.size()) +
           le32(48) + le32(0x00888205) + QByteArray::fromHex("0123456789abcdef") +
           QByteArray(8, '\0') + le16(info.size()) + le16(info.size()) + le32(48 + domain.size()) +
           domain + info;
}
bool validNtlmProof(const QByteArray &message)
{
    if (!message.startsWith(QByteArray("NTLMSSP\0", 8)) || message.size() < 64 ||
        qFromLittleEndian<quint32>(message.data() + 8) != 3)
        return false;
    const auto user = field(message, 36), domain = field(message, 28),
               response = field(message, 20);
    if (user.toUpper() != utf16("USER") || domain.toUpper() != utf16("DOMAIN") ||
        response.size() < 48 || response.mid(16, 2) != QByteArray::fromHex("0101"))
        return false;
    // Independent server-side proof check from MS-NLMP 3.3.2, not a Qt token snapshot.
    const auto passwordHash = QCryptographicHash::hash(utf16("Password"), QCryptographicHash::Md4);
    const auto key = QMessageAuthenticationCode::hash(utf16("USER") + domain, passwordHash,
                                                      QCryptographicHash::Md5);
    const auto proof = QMessageAuthenticationCode::hash(
        QByteArray::fromHex("0123456789abcdef") + response.mid(16), key, QCryptographicHash::Md5);
    return response.left(16) == proof;
}
bool validDigest(const QByteArray &authorization, const QByteArray &method = "CONNECT",
                 const QByteArray &uri = "never-resolve.invalid:443")
{
    if (!authorization.startsWith("Digest "))
        return false;
    QMap<QByteArray, QByteArray> values;
    QRegularExpression expression("([a-zA-Z]+)=(?:\"([^\"]*)\"|([^, ]+))");
    auto matches = expression.globalMatch(QString::fromLatin1(authorization.mid(7)));
    while (matches.hasNext()) {
        const auto match = matches.next();
        values.insert(
            match.captured(1).toLatin1(),
            (match.captured(2).isNull() ? match.captured(3) : match.captured(2)).toLatin1());
    }
    // Quoted-pair escaping and proxy origin-form/absolute-form are equivalent
    // when they designate this same request resource (RFC 7616, 3.4.6).
    values["username"].replace("\\\\", "\\");
    auto origin = QUrl::fromEncoded(uri).toEncoded(QUrl::RemoveScheme | QUrl::RemoveAuthority);
    const bool sameResource =
        values["uri"] == uri || (uri.startsWith("http://") && values["uri"] == origin);
    if (values["username"] != "Domain\\User" || values["realm"] != "echo-test" ||
        values["nonce"] != "test-nonce" || !sameResource || values["qop"] != "auth" ||
        values["nc"] != "00000001" || values["cnonce"].isEmpty()) {
        return false;
    }
    const auto algorithm = values.value("algorithm", "MD5").toUpper();
    if (algorithm != "MD5" && algorithm != "SHA-256")
        return false;
    const auto hash = [&](const QByteArray &bytes) {
        return QCryptographicHash::hash(bytes, algorithm == "SHA-256" ? QCryptographicHash::Sha256
                                                                      : QCryptographicHash::Md5)
            .toHex();
    };
    const auto expected =
        hash(hash("Domain\\User:echo-test:Password") + ":test-nonce:" + values["nc"] + ':' +
             values["cnonce"] + ":auth:" + hash(method + ':' + values["uri"]));
    return values["response"] == expected;
}
std::unique_ptr<QTcpServer> makeProxy(bool tls)
{
    std::unique_ptr<QTcpServer> proxy;
    const auto originalSsl = QSslConfiguration::defaultConfiguration();
    if (tls) {
        QFile certFile(QFINDTESTDATA("../fixtures/network/proxy-test-cert.pem"));
        QFile caFile(QFINDTESTDATA("../fixtures/network/proxy-test-ca.pem"));
        QFile keyFile(QFINDTESTDATA("../fixtures/network/proxy-test-key.pem"));
        if (!certFile.open(QIODevice::ReadOnly) || !caFile.open(QIODevice::ReadOnly) ||
            !keyFile.open(QIODevice::ReadOnly))
            return {};
        auto client = originalSsl;
        client.addCaCertificate(QSslCertificate(caFile.readAll()));
        QSslConfiguration::setDefaultConfiguration(client);
        auto config = originalSsl;
        config.setLocalCertificate(QSslCertificate(certFile.readAll()));
        config.setPrivateKey(QSslKey(keyFile.readAll(), QSsl::Rsa));
        config.setPeerVerifyMode(QSslSocket::VerifyNone);
        auto server = std::make_unique<QSslServer>();
        server->setSslConfiguration(config);
        proxy = std::move(server);
    } else
        proxy = std::make_unique<QTcpServer>();
    proxy->setProxy(QNetworkProxy::NoProxy);
    return proxy;
}
} // namespace
class ProxyAuthenticationTest final : public QObject
{
    Q_OBJECT
private slots:
    void slowHttpReaderReceivesEveryByte_data()
    {
        QTest::addColumn<bool>("tls");
        QTest::newRow("http-proxy") << false;
        QTest::newRow("https-proxy") << true;
    }
    void slowHttpReaderReceivesEveryByte()
    {
        QFETCH(bool, tls);
        const auto originalSsl = QSslConfiguration::defaultConfiguration();
        const auto restore =
            qScopeGuard([&] { QSslConfiguration::setDefaultConfiguration(originalSsl); });
        auto proxy = makeProxy(tls);
        QVERIFY(proxy);
        QVERIFY(proxy->listen(QHostAddress::LocalHost));
        const QByteArray payload(3 * 1024 * 1024, 'r');
        connect(proxy.get(), &QTcpServer::pendingConnectionAvailable, proxy.get(), [&] {
            auto *peer = proxy->nextPendingConnection();
            auto header = std::make_shared<QByteArray>();
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer, header] {
                *header += peer->readAll();
                if (!header->contains("\r\n\r\n"))
                    return;
                peer->write(
                    "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(payload.size()) +
                    "\r\nConnection: close\r\n\r\n" + payload);
                peer->disconnectFromHost();
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules =
            QString(tls ? "https://localhost:%1" : "http://localhost:%1").arg(proxy->serverPort());
        config.username = "route-user";
        transport.apply(config, "route-secret");
        QTcpSocket client;
        client.setReadBufferSize(1024);
        client.setProxy(QNetworkProxy::NoProxy);
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("GET http://never-resolve.invalid/large HTTP/1.1\r\nHost: target\r\n\r\n");
        // Leave the consumer unread, then drain through a small bounded buffer.
        // This is not a latency/performance acceptance threshold.
        QTest::qWait(150);
        QByteArray received;
        QTimer reader;
        connect(&reader, &QTimer::timeout, &reader, [&] { received += client.readAll(); });
        reader.start(0);
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 10000);
        received += client.readAll();
        QVERIFY(received.startsWith("HTTP/1.1 200"));
        QCOMPARE(received.mid(received.indexOf("\r\n\r\n") + 4), payload);
    }
    void ordinaryHttpPreservesResponseAndDoesNotProbe_data()
    {
        QTest::addColumn<QByteArray>("mode");
        for (const auto &mode : {"post", "chunked-post", "origin-401", "redirect", "chunks",
                                 "broken-chunks", "gzip", "head", "rejected-proxy"})
            QTest::newRow(mode) << QByteArray(mode);
    }
    void ordinaryHttpPreservesResponseAndDoesNotProbe()
    {
        QFETCH(QByteArray, mode);
        QTcpServer proxy;
        proxy.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        int requests = 0;
        const QByteArray payload = (mode == "post" || mode == "chunked-post")
                                       ? QByteArray(2 * 1024 * 1024, 'q')
                                       : QByteArray();
        const auto gzip = QByteArray::fromHex("1f8b0800000000000213cbcf060047dddc7902000000");
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            auto *peer = proxy.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer, buffer] {
                *buffer += peer->readAll();
                QByteArray header, body;
                while (takeRequest(*buffer, header, body)) {
                    QCOMPARE(body, payload);
                    ++requests;
                    QVERIFY(!header.startsWith("CONNECT"));
                    QVERIFY(!header.toLower().contains("x-must-strip:"));
                    QVERIFY(!header.toLower().contains("cookie:"));
                    QVERIFY(!header.toLower().contains("\nauthorization: basic"));
                    if (mode == "rejected-proxy") {
                        peer->write(
                            "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: "
                            "Basic realm=\"reject\"\r\nContent-Length: 0\r\nProxy-Connection: "
                            "keep-alive\r\n\r\n");
                        continue;
                    }
                    if (mode == "origin-401")
                        peer->write("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic "
                                    "realm=\"origin\"\r\nTransfer-Encoding: chunked\r\nSet-Cookie: "
                                    "first=a\r\nSet-Cookie: second=b\r\nConnection: X-Upstream, "
                                    "close\r\nX-Upstream: hidden\r\n\r\n6\r\ndenied\r\n0\r\n\r\n");
                    else if (mode == "redirect")
                        peer->write("HTTP/1.1 302 Found\r\nLocation: "
                                    "http://must-not-follow.invalid/next\r\nContent-Length: "
                                    "0\r\nConnection: close\r\n\r\n");
                    else if (mode == "chunks" || mode == "broken-chunks")
                        peer->write("HTTP/1.1 404 Not Found\r\nTransfer-Encoding: "
                                    "chunked\r\nConnection: close\r\n\r\n2\r\nok\r\n" +
                                    QByteArray(mode == "chunks" ? "0\r\n\r\n" : ""));
                    else if (mode == "gzip")
                        peer->write(
                            "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: " +
                            QByteArray::number(gzip.size()) + "\r\nConnection: close\r\n\r\n" +
                            gzip);
                    else if (mode == "head")
                        peer->write(
                            "HTTP/1.1 200 OK\r\nContent-Length: 999\r\nConnection: close\r\n\r\n");
                    else
                        peer->write(
                            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
                    peer->disconnectFromHost();
                }
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("http://127.0.0.1:%1").arg(proxy.serverPort());
        config.username = "route-user";
        transport.apply(config, "route-secret");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        QByteArray received;
        connect(&client, &QTcpSocket::readyRead, &client, [&] { received += client.readAll(); });
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        const QByteArray verb = (mode == "post" || mode == "chunked-post") ? "POST"
                                : mode == "head"                           ? "HEAD"
                                                                           : "GET";
        const auto header =
            verb +
            QByteArray(" http://never-resolve.invalid/request?must-remain=query HTTP/1.1\r\nHost: "
                       "target\r\nConnection: X-Must-Strip\r\nX-Must-Strip: hidden\r\n");
        if (mode == "chunked-post")
            client.write(header + "Transfer-Encoding: chunked\r\n\r\n" +
                         QByteArray::number(payload.size(), 16) + "\r\n" + payload +
                         "\r\n0\r\n\r\n");
        else
            client.write(header + "Content-Length: " + QByteArray::number(payload.size()) +
                         "\r\n\r\n" + payload);
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 5000);
        if (mode == "rejected-proxy") {
            QVERIFY(requests >= 2 && requests <= 3);
            QVERIFY(received.startsWith("HTTP/1.1 502"));
            QVERIFY(!received.toLower().contains("proxy-authenticate:"));
            return;
        }
        QCOMPARE(requests, 1);
        const auto body = received.mid(received.indexOf("\r\n\r\n") + 4);
        if (mode == "origin-401") {
            QVERIFY(received.startsWith("HTTP/1.1 401"));
            QVERIFY(received.toLower().contains("www-authenticate: basic"));
            QCOMPARE(received.toLower().count("set-cookie:"), 2);
            QVERIFY(!received.toLower().contains("x-upstream:"));
            QVERIFY(body.contains("denied"));
            QVERIFY(body.endsWith("0\r\n\r\n"));
        } else if (mode == "redirect") {
            QVERIFY(received.startsWith("HTTP/1.1 302"));
            QVERIFY(received.contains("http://must-not-follow.invalid/next"));
            QVERIFY(body.isEmpty());
        } else if (mode == "chunks" || mode == "broken-chunks") {
            QVERIFY(received.startsWith("HTTP/1.1 404"));
            QVERIFY(body.contains("ok"));
            QCOMPARE(body.endsWith("0\r\n\r\n"), mode == "chunks");
        } else if (mode == "gzip") {
            QVERIFY(received.toLower().contains("content-encoding: gzip"));
            QCOMPARE(body, gzip);
        } else if (mode == "head") {
            QVERIFY(received.toLower().contains("content-length: 999"));
            QVERIFY(body.isEmpty());
        } else
            QCOMPARE(body, QByteArray("ok"));
    }
    void nonIdempotentHttpDropDoesNotReplay_data()
    {
        QTest::addColumn<bool>("empty");
        QTest::newRow("empty-post") << true;
        QTest::newRow("binary-post") << false;
    }
    void nonIdempotentHttpDropDoesNotReplay()
    {
        QFETCH(bool, empty);
        QTcpServer proxy;
        proxy.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        const QByteArray payload = empty ? QByteArray() : QByteArray(2 * 1024 * 1024, 'p');
        int executions = 0;
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            auto *peer = proxy.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer, buffer] {
                *buffer += peer->readAll();
                while (buffer->contains("\r\n\r\n")) {
                    const auto end = buffer->indexOf("\r\n\r\n") + 4;
                    const auto header = buffer->left(end);
                    qint64 length = 0;
                    for (const auto &line : header.split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(line.indexOf(':') + 1).trimmed().toLongLong();
                    if (buffer->size() < end + length)
                        return;
                    const auto body = buffer->mid(end, length);
                    buffer->remove(0, end + length);
                    if (!header.toLower().contains("proxy-authorization:")) {
                        peer->write(
                            "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: "
                            "Basic realm=\"test\"\r\nContent-Length: 0\r\nProxy-Connection: "
                            "keep-alive\r\n\r\n");
                        continue;
                    }
                    QCOMPARE(body, payload);
                    ++executions;
                    // The origin operation happened; its response was lost.
                    peer->abort();
                }
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("http://127.0.0.1:%1").arg(proxy.serverPort());
        config.username = "route-user";
        transport.apply(config, "route-secret");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        QByteArray received;
        connect(&client, &QTcpSocket::readyRead, &client, [&] { received += client.readAll(); });
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("POST http://never-resolve.invalid/mutate HTTP/1.1\r\nHost: "
                     "target\r\nContent-Length: " +
                     QByteArray::number(payload.size()) + "\r\n\r\n" + payload);
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 5000);
        QCOMPARE(executions, 1);
        QVERIFY(received.startsWith("HTTP/1.1 502"));
    }
    void incompleteUploadCancellationReleasesIoAndSpool_data()
    {
        QTest::addColumn<bool>("policyChange");
        QTest::newRow("disconnect") << false;
        QTest::newRow("policy") << true;
    }
    void incompleteUploadCancellationReleasesIoAndSpool()
    {
        QFETCH(bool, policyChange);
        QDir temporary(QDir::tempPath());
        const auto files = [&] {
            return temporary.entryList({"echomusic-http-*"}, QDir::Dirs | QDir::NoDotAndDotDot);
        };
        const auto before = files();
        QTcpServer proxy;
        proxy.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("http://127.0.0.1:%1").arg(proxy.serverPort());
        config.username = "route-user";
        transport.apply(config, "route-secret");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("POST http://never-resolve.invalid/upload HTTP/1.1\r\nHost: "
                     "target\r\nContent-Length: 4194304\r\n\r\n" +
                     QByteArray(2 * 1024 * 1024, 'u'));
        QTRY_VERIFY(proxy.hasPendingConnections());
        auto *peer = proxy.nextPendingConnection();
        QTRY_VERIFY(files().size() > before.size());
        // Upload must already be flowing before the declared body is complete.
        QTRY_VERIFY(peer->bytesAvailable() > 4096);
        const auto partial = peer->readAll();
        QVERIFY(partial.startsWith("POST "));
        QVERIFY(partial.contains(QByteArray(4096, 'u')));
        QVERIFY(partial.size() < 4194304);
        if (policyChange)
            transport.apply(NetworkConfiguration{}, {});
        else
            client.abort();
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
        QTRY_COMPARE(files(), before);
        peer->deleteLater();
    }
    void earlyChallengeRewindsWhileUploadIsStillArriving_data()
    {
        QTest::addColumn<bool>("chunked");
        QTest::newRow("fixed") << false;
        QTest::newRow("chunked") << true;
    }
    void earlyChallengeRewindsWhileUploadIsStillArriving()
    {
        QFETCH(bool, chunked);
        QTcpServer proxy;
        proxy.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        const QByteArray payload(2 * 1024 * 1024, 'e');
        bool authenticated = false;
        int accepted = 0, challenges = 0;
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            auto *peer = proxy.nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            auto rejected = std::make_shared<bool>(false);
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer, buffer, rejected] {
                *buffer += peer->readAll();
                const auto boundary = buffer->indexOf("\r\n\r\n");
                if (boundary < 0 || *rejected)
                    return;
                const auto request = buffer->left(boundary + 4);
                QByteArray authorization;
                for (const auto &line : request.split('\n'))
                    if (line.toLower().startsWith("proxy-authorization:"))
                        authorization = line.mid(line.indexOf(':') + 1).trimmed();
                if (!validDigest(authorization, "POST", "http://never-resolve.invalid/early")) {
                    // Reject on headers, without consuming the unfinished upload.
                    ++challenges;
                    *rejected = true;
                    peer->write("HTTP/1.1 407 Proxy Authentication Required\r\n"
                                "Proxy-Authenticate: Digest realm=\"echo-test\", "
                                "nonce=\"test-nonce\", qop=\"auth\", algorithm=SHA-256\r\n"
                                "Content-Length: 0\r\nConnection: close\r\n\r\n");
                    peer->disconnectFromHost();
                    return;
                }
                authenticated = true;
                QByteArray header, body;
                if (!takeRequest(*buffer, header, body))
                    return;
                QCOMPARE(body, payload);
                ++accepted;
                peer->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
                peer->disconnectFromHost();
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("http://127.0.0.1:%1").arg(proxy.serverPort());
        config.username = "Domain\\User";
        transport.apply(config, "Password");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        QByteArray received;
        connect(&client, &QTcpSocket::readyRead, &client, [&] { received += client.readAll(); });
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        const auto prefix = payload.left(65536), remaining = payload.mid(65536);
        const auto chunk = [](const QByteArray &bytes) {
            return QByteArray::number(bytes.size(), 16) + "\r\n" + bytes + "\r\n";
        };
        client.write(
            QByteArray("POST http://never-resolve.invalid/early HTTP/1.1\r\nHost: target\r\n") +
            (chunked ? QByteArray("Transfer-Encoding: chunked\r\n\r\n")
                     : "Content-Length: " + QByteArray::number(payload.size()) + "\r\n\r\n") +
            (chunked ? chunk(prefix) : prefix));
        QTRY_VERIFY_WITH_TIMEOUT(authenticated, 5000);
        QCOMPARE(accepted, 0);
        client.write(chunked ? chunk(remaining) + "0\r\n\r\n" : remaining);
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState, 5000);
        QVERIFY(received.endsWith("\r\n\r\nok"));
        QCOMPARE(accepted, 1);
        QCOMPARE(challenges, 1);
    }
    void rewindableBodyKeepsSmallRequestsInMemoryAndRemovesLargeFiles()
    {
        HttpRequestBody small;
        QVERIFY(small.begin({{"Content-Length", "6"}}));
        QVERIFY(small.append("secret"));
        QVERIFY(small.complete());
        auto *memory = small.rewind();
        QVERIFY(memory);
        QVERIFY(!qobject_cast<QFile *>(memory));
        QCOMPARE(memory->readAll(), QByteArray("secret"));
        QCOMPARE(small.rewind()->readAll(), QByteArray("secret"));
        HttpRequestBody large;
        const QByteArray payload(2 * 1024 * 1024, 's');
        QVERIFY(large.begin({{"Content-Length", QByteArray::number(payload.size())}}));
        for (qsizetype i = 0; i < payload.size(); i += 32768)
            QVERIFY(large.append(payload.mid(i, 32768)));
        QVERIFY(large.complete());
        auto *file = qobject_cast<QFile *>(large.rewind());
        QVERIFY(file);
        const auto name = file->fileName();
        const auto directory = QFileInfo(name).absolutePath();
        QVERIFY(QFile::exists(name));
#ifdef Q_OS_WIN
        QNtfsPermissionCheckGuard permissionChecks;
#endif
        const auto forbidden = QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                               QFileDevice::ExeGroup | QFileDevice::ReadOther |
                               QFileDevice::WriteOther | QFileDevice::ExeOther;
        QVERIFY(!(QFileInfo(directory).permissions() & forbidden));
        QVERIFY(!(file->permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                         QFileDevice::ReadOther | QFileDevice::WriteOther)));
        QCOMPARE(file->readAll(), payload);
        QCOMPARE(large.rewind()->readAll(), payload);
        large.clear();
        QVERIFY(!QFile::exists(name));
        QVERIFY(!QFile::exists(directory));
        HttpRequestBody chunked;
        QVERIFY(chunked.begin({{"Transfer-Encoding", "chunked"}}));
        for (const auto c : (QByteArray("3;ext=yes\r\na", 12) + QByteArray(1, '\0') +
                             "b\r\n2\r\ncd\r\n0\r\nX-Checksum: value\r\n\r\n"))
            QVERIFY(chunked.append(QByteArray(1, c)));
        QVERIFY(chunked.complete());
        QCOMPARE(chunked.rewind()->readAll(), QByteArray("a\0bcd", 5));
        QCOMPARE(chunked.trailers(), HttpRequestBody::Headers({{"x-checksum", "value"}}));
    }
    void bodyFramingRejectsAmbiguousRequests()
    {
        HttpRequestBody ambiguous;
        QVERIFY(!ambiguous.begin({{"Content-Length", "3"}, {"Transfer-Encoding", "chunked"}}));
        QCOMPARE(ambiguous.errorStatus(), 400);
        HttpRequestBody duplicate;
        QVERIFY(!duplicate.begin({{"Content-Length", "3"}, {"Content-Length", "4"}}));
        HttpRequestBody negative;
        QVERIFY(!negative.begin({{"Content-Length", "-1"}}));
        HttpRequestBody invalidChunk;
        QVERIFY(invalidChunk.begin({{"Transfer-Encoding", "chunked"}}));
        QVERIFY(!invalidChunk.append("+3\r\nabc\r\n0\r\n\r\n"));
        HttpRequestBody trailer;
        QVERIFY(trailer.begin({{"Transfer-Encoding", "chunked"}}));
        QVERIFY(!trailer.append("0\r\nAuthorization: must-not-be-reinterpreted\r\n\r\n"));
        HttpRequestBody incomplete;
        QVERIFY(incomplete.begin({{"Content-Length", "4"}}));
        QVERIFY(incomplete.append("abc"));
        QVERIFY(!incomplete.complete());
        QVERIFY(!incomplete.rewind());
    }
    void httpAuthentication_data()
    {
        QTest::addColumn<QByteArray>("method");
        QTest::addColumn<bool>("tls");
        QTest::addColumn<QByteArray>("verb");
        for (const auto &method : {QByteArray("Basic"), QByteArray("Digest"),
                                   QByteArray("Digest-SHA256"), QByteArray("NTLM")})
            for (const bool tls : {false, true})
                for (const auto &verb :
                     {QByteArray("GET"), QByteArray("POST"), QByteArray("PATCH")})
                    QTest::newRow((method + (tls ? "-tls-" : "-tcp-") + verb).constData())
                        << method << tls << verb;
    }
    void httpAuthentication()
    {
        QFETCH(QByteArray, method);
        QFETCH(bool, tls);
        QFETCH(QByteArray, verb);
        const auto originalSsl = QSslConfiguration::defaultConfiguration();
        const auto restore =
            qScopeGuard([&] { QSslConfiguration::setDefaultConfiguration(originalSsl); });
        auto proxy = makeProxy(tls);
        QVERIFY(proxy);
        QVERIFY(proxy->listen(QHostAddress::LocalHost));
        const QByteArray payload =
            verb == "GET" ? QByteArray() : QByteArray(2 * 1024 * 1024, '\x5a');
        const QByteArray target = "http://never-resolve.invalid/upload?q=1";
        int attempts = 0, accepted = 0;
        connect(proxy.get(), &QTcpServer::pendingConnectionAvailable, proxy.get(), [&] {
            auto *peer = proxy->nextPendingConnection();
            auto buffer = std::make_shared<QByteArray>();
            const auto read = [&, peer, buffer] {
                *buffer += peer->readAll();
                QByteArray header, body;
                while (takeRequest(*buffer, header, body)) {
                    QByteArray authorization;
                    for (const auto &line : header.split('\n'))
                        if (line.toLower().startsWith("proxy-authorization:"))
                            authorization = line.mid(line.indexOf(':') + 1).trimmed();
                    ++attempts;
                    QVERIFY(header.startsWith(verb + ' ' + target + " HTTP/1.1"));
                    QVERIFY(!header.contains("must-not-forward"));
                    QVERIFY(header.toLower().contains("authorization: bearer origin-token"));
                    if (!body.isEmpty() || payload.isEmpty())
                        QCOMPARE(body, payload);
                    else {
                        // NTLM's Type-1 negotiation may suppress the upload.
                        // The authenticated Type-3 request must carry it fully.
                        QVERIFY(authorization.startsWith("NTLM "));
                        const auto token = QByteArray::fromBase64(authorization.mid(5));
                        QVERIFY(token.size() >= 12);
                        QCOMPARE(qFromLittleEndian<quint32>(token.data() + 8), quint32(1));
                    }
                    bool valid = false;
                    auto challenge = method;
                    if (method == "Basic") {
                        valid = authorization ==
                                "Basic " + QByteArray("Domain\\User:Password").toBase64();
                        challenge += " realm=\"echo-test\"";
                    } else if ((method == "Digest" || method == "Digest-SHA256")) {
                        valid = validDigest(authorization, verb, target);
                        challenge = "Digest realm=\"echo-test\", nonce=\"test-nonce\", "
                                    "qop=\"auth\", algorithm=" +
                                    QByteArray(method == "Digest-SHA256" ? "SHA-256" : "MD5");
                    } else if (authorization.startsWith("NTLM ")) {
                        const auto token = QByteArray::fromBase64(authorization.mid(5));
                        if (token.size() >= 12 && qFromLittleEndian<quint32>(token.data() + 8) == 1)
                            challenge += ' ' + ntlmChallenge().toBase64();
                        else
                            valid = validNtlmProof(token);
                    }
                    if (valid) {
                        QCOMPARE(body, payload);
                        if (verb == "PATCH")
                            QVERIFY(header.toLower().contains("x-checksum: retained"));
                        ++accepted;
                        peer->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nSet-Cookie: "
                                    "upstream=value\r\nConnection: close\r\n\r\nok");
                        peer->disconnectFromHost();
                    } else
                        peer->write(
                            "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: " +
                            challenge +
                            "\r\nContent-Length: 9\r\nProxy-Connection: "
                            "keep-alive\r\n\r\nchallenge");
                }
            };
            connect(peer, &QTcpSocket::readyRead, peer, read);
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
            if (peer->bytesAvailable())
                read();
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules =
            QString(tls ? "https://localhost:%1" : "http://localhost:%1").arg(proxy->serverPort());
        config.username = "Domain\\User";
        transport.apply(config, "Password");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        QByteArray received;
        connect(&client, &QTcpSocket::readyRead, &client, [&] { received += client.readAll(); });
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        QByteArray header = verb + ' ' + target +
                            " HTTP/1.1\r\nHost: never-resolve.invalid\r\nAuthorization: Bearer "
                            "origin-token\r\nProxy-Authorization: must-not-forward\r\n";
        if (verb == "PATCH") {
            client.write(header + "Transfer-Encoding: chunked\r\nTrailer: X-Checksum\r\n\r\n");
            for (qsizetype offset = 0; offset < payload.size(); offset += 32768) {
                const auto part = payload.mid(offset, 32768);
                client.write(QByteArray::number(part.size(), 16) + ";test=yes\r\n" + part + "\r\n");
            }
            client.write("0\r\nX-Checksum: retained\r\n\r\n");
        } else
            client.write(header + "Content-Length: " + QByteArray::number(payload.size()) +
                         "\r\n\r\n" + payload);
        QTRY_VERIFY2_WITH_TIMEOUT(received.endsWith("\r\n\r\nok"), received.constData(), 10000);
        QVERIFY(received.startsWith("HTTP/1.1 200"));
        QVERIFY(!received.contains("407"));
        QVERIFY(received.toLower().contains("set-cookie: upstream=value"));
        QCOMPARE(accepted, 1);
        QVERIFY(attempts >= 2 && attempts <= 4);
        QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
    }
    void ipv6ConnectPreservesAuthority()
    {
        QTcpServer proxy;
        proxy.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        QByteArray request;
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            auto *peer = proxy.nextPendingConnection();
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer] {
                request += peer->readAll();
                if (request.contains("\r\n\r\n"))
                    peer->write("HTTP/1.1 200 Connection Established\r\n\r\n");
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("http://127.0.0.1:%1").arg(proxy.serverPort());
        config.username = "route-user";
        transport.apply(config, "route-secret");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("CONNECT [2001:db8::1]:443 HTTP/1.1\r\nHost: [2001:db8::1]\r\n\r\n");
        QTRY_VERIFY(client.bytesAvailable());
        QVERIFY(client.readAll().startsWith("HTTP/1.1 200"));
        QVERIFY2(request.startsWith("CONNECT [2001:db8::1]:443 HTTP/1.1"), request.constData());
    }
    void proxyRejectionNeverFallsBack_data()
    {
        QTest::addColumn<int>("status");
        for (const int status : {403, 404, 405, 407, 500, 503})
            QTest::newRow(qPrintable(QString::number(status))) << status;
    }
    void proxyRejectionNeverFallsBack()
    {
        QFETCH(int, status);
        QTcpServer proxy, origin;
        proxy.setProxy(QNetworkProxy::NoProxy);
        origin.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        QVERIFY(origin.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            auto *peer = proxy.nextPendingConnection();
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer] {
                const auto request = peer->readAll();
                QVERIFY(request.startsWith("CONNECT "));
                ++requests;
                peer->write("HTTP/1.1 " + QByteArray::number(status) +
                            " Rejected\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                peer->disconnectFromHost();
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("http://127.0.0.1:%1,direct://").arg(proxy.serverPort());
        config.bypassRules = "<-loopback>";
        config.username = "route-user";
        transport.apply(config, "route-secret");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("CONNECT 127.0.0.1:" + QByteArray::number(origin.serverPort()) +
                     " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QTRY_VERIFY(client.bytesAvailable());
        QVERIFY(client.readAll().startsWith("HTTP/1.1 502"));
        // Qt may retry the CONNECT control exchange while exhausting target
        // address candidates; no origin/business payload may be sent.
        QVERIFY(requests >= 1 && requests <= 2);
        QVERIFY(!origin.hasPendingConnections());
    }
    void unavailableTlsProxyUsesExplicitFallback()
    {
        QTcpServer unavailable, origin;
        unavailable.setProxy(QNetworkProxy::NoProxy);
        origin.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(unavailable.listen(QHostAddress::LocalHost));
        const auto port = unavailable.serverPort();
        unavailable.close();
        QVERIFY(origin.listen(QHostAddress::LocalHost));
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("https://127.0.0.1:%1,direct://").arg(port);
        config.bypassRules = "<-loopback>";
        config.username = "route-user";
        transport.apply(config, "route-secret");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("CONNECT 127.0.0.1:" + QByteArray::number(origin.serverPort()) +
                     " HTTP/1.1\r\nHost: localhost\r\n\r\n");
        QTRY_VERIFY(origin.hasPendingConnections());
        auto *peer = origin.nextPendingConnection();
        QTRY_VERIFY(client.bytesAvailable());
        QVERIFY(client.readAll().startsWith("HTTP/1.1 200"));
        QVERIFY(!peer->bytesAvailable());
        client.write("origin-payload");
        QTRY_VERIFY(peer->bytesAvailable());
        QCOMPARE(peer->readAll(), QByteArray("origin-payload"));
        client.abort();
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
        peer->deleteLater();
    }
    void cancellingHandshakeClosesProxyIo_data()
    {
        QTest::addColumn<bool>("policyChange");
        QTest::newRow("downstream-disconnect") << false;
        QTest::newRow("policy-replacement") << true;
    }
    void cancellingHandshakeClosesProxyIo()
    {
        QFETCH(bool, policyChange);
        QTcpServer proxy;
        proxy.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules = QString("http://127.0.0.1:%1").arg(proxy.serverPort());
        config.username = "route-user";
        transport.apply(config, "route-secret");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("CONNECT never-resolve.invalid:443 HTTP/1.1\r\nHost: target\r\n\r\n");
        QTRY_VERIFY(proxy.hasPendingConnections());
        auto *peer = proxy.nextPendingConnection();
        QTRY_VERIFY(peer->bytesAvailable());
        QVERIFY(peer->readAll().startsWith("CONNECT "));
        if (policyChange)
            transport.apply(NetworkConfiguration{}, {});
        else
            client.abort();
        QTRY_COMPARE(peer->state(), QAbstractSocket::UnconnectedState);
        peer->deleteLater();
    }
    void connectAuthentication_data()
    {
        QTest::addColumn<QByteArray>("method");
        QTest::addColumn<bool>("tls");
        QTest::addColumn<bool>("reject");
        QTest::addColumn<QByteArray>("authority");
        for (const auto &method : {QByteArray("Basic"), QByteArray("Digest"),
                                   QByteArray("Digest-SHA256"), QByteArray("NTLM")})
            for (const bool tls : {false, true})
                for (const bool reject : {false, true})
                    for (const auto &authority :
                         {QByteArray("never-resolve.invalid:443"), QByteArray("[2001:db8::1]:443")})
                        QTest::newRow((method + (tls ? "-tls" : "-tcp") +
                                       (reject ? "-reject" : "-accept") +
                                       (authority.startsWith('[') ? "-ipv6" : "-name"))
                                          .constData())
                            << method << tls << reject << authority;
    }
    void connectAuthentication()
    {
        QFETCH(QByteArray, method);
        QFETCH(bool, tls);
        QFETCH(bool, reject);
        QFETCH(QByteArray, authority);
        const auto originalSsl = QSslConfiguration::defaultConfiguration();
        const auto restore =
            qScopeGuard([&] { QSslConfiguration::setDefaultConfiguration(originalSsl); });
        auto proxy = makeProxy(tls);
        QVERIFY(proxy);
        proxy->setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy->listen(QHostAddress::LocalHost));
        int requests = 0, accepted = 0, proofs = 0;
        connect(proxy.get(), &QTcpServer::pendingConnectionAvailable, proxy.get(), [&] {
            auto *peer = proxy->nextPendingConnection();
            QVERIFY(peer);
            auto buffer = std::make_shared<QByteArray>();
            auto authenticated = std::make_shared<bool>(false);
            const auto read = [&, peer, buffer, authenticated] {
                if (*authenticated) {
                    peer->write(peer->readAll());
                    return;
                }
                *buffer += peer->readAll();
                while (buffer->contains("\r\n\r\n")) {
                    const auto end = buffer->indexOf("\r\n\r\n") + 4;
                    const auto request = buffer->left(end);
                    buffer->remove(0, end);
                    ++requests;
                    QVERIFY(request.startsWith("CONNECT " + authority + " HTTP/1.1"));
                    QByteArray authorization;
                    for (const auto &line : request.split('\n'))
                        if (line.toLower().startsWith("proxy-authorization:"))
                            authorization = line.mid(line.indexOf(':') + 1).trimmed();
                    bool valid = false;
                    auto challenge = method;
                    if (method == "Basic") {
                        valid = authorization ==
                                "Basic " + QByteArray("Domain\\User:Password").toBase64();
                        challenge += " realm=\"echo-test\"";
                    } else if ((method == "Digest" || method == "Digest-SHA256")) {
                        valid = validDigest(authorization, "CONNECT", authority);
                        challenge = "Digest realm=\"echo-test\", nonce=\"test-nonce\", "
                                    "qop=\"auth\", algorithm=" +
                                    QByteArray(method == "Digest-SHA256" ? "SHA-256" : "MD5");
                    } else if (authorization.startsWith("NTLM ")) {
                        const auto token = QByteArray::fromBase64(authorization.mid(5));
                        if (token.size() >= 12 && qFromLittleEndian<quint32>(token.data() + 8) == 1)
                            challenge += ' ' + ntlmChallenge().toBase64();
                        else
                            valid = validNtlmProof(token);
                    }
                    if (valid)
                        ++proofs;
                    if (valid && !reject) {
                        ++accepted;
                        *authenticated = true;
                        peer->write("HTTP/1.1 200 Connection Established\r\n\r\nauthenticated");
                    } else {
                        peer->write(
                            "HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: " +
                            challenge +
                            "\r\nContent-Length: 0\r\nProxy-Connection: keep-alive\r\n\r\n");
                    }
                }
            };
            connect(peer, &QTcpSocket::readyRead, peer, read);
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
            if (peer->bytesAvailable())
                read();
        });
        NetworkTransport transport;
        NetworkConfiguration config;
        config.mode = NetworkConfiguration::Mode::FixedServers;
        config.proxyRules =
            QString(tls ? "https://localhost:%1" : "http://localhost:%1").arg(proxy->serverPort());
        config.username = "Domain\\User";
        transport.apply(config, "Password");
        QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);
        QByteArray received;
        connect(&client, &QTcpSocket::readyRead, &client, [&] { received += client.readAll(); });
        client.connectToHost(QHostAddress::LocalHost, quint16(transport.endpoint().port()));
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write(
            "CONNECT " + authority +
            " HTTP/1.1\r\nHost: target\r\nProxy-Authorization: Basic must-not-forward\r\n\r\n");
        if (reject) {
            QTRY_VERIFY2_WITH_TIMEOUT(received.startsWith("HTTP/1.1 502"), received.constData(),
                                      5000);
            QCOMPARE(accepted, 0);
            QVERIFY(requests <= 6);
        } else {
            QTRY_VERIFY2_WITH_TIMEOUT(received.contains("authenticated"), received.constData(),
                                      5000);
            QCOMPARE(accepted, 1);
            QCOMPARE(proofs, 1);
            QVERIFY(received.startsWith("HTTP/1.1 200"));
            received.clear();
            const QByteArray payload(512 * 1024, 'a');
            client.write(payload);
            QTRY_COMPARE_WITH_TIMEOUT(received.size(), payload.size(), 5000);
            QCOMPARE(received, payload);
            transport.apply(NetworkConfiguration{}, {});
            QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
        }
    }
};
int main(int argc, char **argv)
{
    initializeNetworkEnvironment();
    QCoreApplication application(argc, argv);
    ProxyAuthenticationTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ProxyAuthenticationTest.moc"
