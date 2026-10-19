#include "BrowserVerificationSession.h"
#include "VerificationService.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <cstring>

namespace EchoMusic
{
namespace
{
constexpr qsizetype maximumRequest = 32768;
constexpr qsizetype maximumHeader = 8192;
} // namespace
BrowserVerificationSession::BrowserVerificationSession(VerificationService &service,
                                                       QObject *parent)
    : QObject(parent), m_service(service)
{
    m_server.setProxy(QNetworkProxy::NoProxy);
    m_server.setMaxPendingConnections(8);
    connect(&m_server, &QTcpServer::newConnection, this,
            &BrowserVerificationSession::acceptConnections);
    m_expiry.setSingleShot(true);
    m_expiry.setInterval(180000);
    connect(&m_expiry, &QTimer::timeout, this, [this] {
        stop();
        emit expired();
    });
    connect(&service, &VerificationService::changed, this, [this] {
        if (active() && (!m_service.open() || m_generation != m_service.generation() ||
                         m_service.state() == VerificationService::State::Verifying))
            stop();
    });
}
BrowserVerificationSession::~BrowserVerificationSession()
{
    stop();
}
bool BrowserVerificationSession::active() const
{
    return m_server.isListening();
}
QUrl BrowserVerificationSession::start()
{
    stop();
    if (!m_service.open() || m_service.provider() != QStringLiteral("tencent") ||
        m_service.verificationType() != 23 || m_service.appId().isEmpty() ||
        (m_service.state() != VerificationService::State::Ready &&
         m_service.state() != VerificationService::State::Error))
        return {};
    if (!m_server.listen(QHostAddress::LocalHost, 0))
        return {};
    QByteArray random(32, Qt::Uninitialized);
    for (qsizetype offset = 0; offset < random.size(); offset += 4) {
        const auto value = QRandomGenerator::system()->generate();
        memcpy(random.data() + offset, &value, sizeof(value));
    }
    m_nonce = random.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    m_generation = m_service.generation();
    m_origin = "http://127.0.0.1:" + QByteArray::number(m_server.serverPort());
    m_expiry.start();
    emit activeChanged();
    if (!active() || !m_service.open() || m_generation != m_service.generation())
        return {}; // A direct signal may cancel or replace this event.
    return QUrl(QString::fromLatin1(m_origin + "/verification#" + m_nonce));
}
void BrowserVerificationSession::stop()
{
    const bool wasActive = active();
    m_server.close();
    m_expiry.stop();
    m_nonce.clear();
    m_origin.clear();
    m_generation = 0;
    const auto peers = m_buffers.keys();
    m_buffers.clear();
    for (auto *peer : peers)
        peer->disconnectFromHost();
    if (wasActive)
        emit activeChanged();
}
void BrowserVerificationSession::acceptConnections()
{
    while (auto *peer = m_server.nextPendingConnection()) {
        peer->setParent(this);
        peer->setReadBufferSize(maximumRequest + 1);
        if (m_buffers.size() >= 8) {
            peer->disconnectFromHost();
            peer->deleteLater();
            continue;
        }
        m_buffers.insert(peer, {});
        connect(peer, &QTcpSocket::readyRead, this, [this, peer] { read(peer); });
        connect(peer, &QTcpSocket::disconnected, this, [this, peer] {
            m_buffers.remove(peer);
            peer->deleteLater();
        });
        QTimer::singleShot(5000, peer, [peer] { peer->disconnectFromHost(); });
    }
}
void BrowserVerificationSession::respond(QTcpSocket *peer, int status, const QByteArray &body,
                                         const QByteArray &contentType)
{
    m_buffers.remove(peer);
    peer->write("HTTP/1.1 " + QByteArray::number(status) + " Response\r\nContent-Type: " +
                contentType + "\r\nContent-Length: " + QByteArray::number(body.size()) +
                "\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\n"
                "X-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n" +
                body);
    peer->disconnectFromHost();
}
void BrowserVerificationSession::read(QTcpSocket *peer)
{
    auto it = m_buffers.find(peer);
    if (it == m_buffers.end())
        return;
    it.value() += peer->readAll();
    const auto &request = it.value();
    const auto headerEnd = request.indexOf("\r\n\r\n");
    if (request.size() > maximumRequest || (headerEnd < 0 && request.size() > maximumHeader) ||
        headerEnd > maximumHeader) {
        respond(peer, 413, "{}");
        return;
    }
    if (headerEnd < 0)
        return;
    const auto lines = request.left(headerEnd).split('\n');
    const auto first = lines.value(0).trimmed().split(' ');
    QHash<QByteArray, QByteArray> headers;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const auto line = lines[i].trimmed();
        const auto colon = line.indexOf(':');
        const auto key = line.left(colon).toLower();
        if (colon <= 0 || headers.contains(key)) {
            respond(peer, 400, "{}");
            return;
        }
        headers.insert(key, line.mid(colon + 1).trimmed());
    }
    if (!active() || first.size() != 3 || first[2] != "HTTP/1.1" ||
        headers.value("host") != m_origin.mid(7) || headers.contains("transfer-encoding")) {
        respond(peer, 403, "{}");
        return;
    }
    if (first[0] == "GET" && first[1] == "/verification") {
        QFile page(QStringLiteral(":/echo/auth/browser-verification.html"));
        if (!page.open(QIODevice::ReadOnly)) {
            respond(peer, 500, "{}");
            return;
        }
        auto html = page.readAll();
        // VerificationService accepts only a decimal AppId; the stable string
        // marker survives HTML/JavaScript formatting without widening the bridge.
        html.replace("ECHO_CAPTCHA_APP_ID", m_service.appId().toUtf8());
        respond(peer, 200, html, "text/html; charset=utf-8");
        return;
    }
    if (first[0] != "POST" || first[1] != "/result") {
        respond(peer, 404, "{}");
        return;
    }
    if (headers.value("origin") != m_origin || headers.value("x-echo-verification") != m_nonce ||
        headers.value("content-type") != "application/json") {
        respond(peer, 403, "{}");
        return;
    }
    bool validLength = false;
    const auto length = headers.value("content-length").toLongLong(&validLength);
    if (validLength && length > maximumRequest - headerEnd - 4) {
        respond(peer, 413, "{}");
        return;
    }
    if (!validLength || length < 1) {
        respond(peer, 400, "{}");
        return;
    }
    if (request.size() < headerEnd + 4 + length)
        return;
    if (request.size() != headerEnd + 4 + length) {
        respond(peer, 400, "{}");
        return;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(request.mid(headerEnd + 4, length), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        respond(peer, 400, "{}");
        return;
    }
    const auto data = document.object();
    if (data.value("event") == QStringLiteral("ready")) {
        respond(peer, 200, "{\"accepted\":true}");
        emit sdkReady();
        return;
    }
    const auto ret = data.value("ret");
    const auto code = data.value("errorCode");
    const auto ticket = data.value("ticket").toString();
    const auto randstr = data.value("randstr").toString();
    if (ret.isDouble() && ret.toDouble() == 2) {
        respond(peer, 200, "{\"cancelled\":true}");
        stop();
        m_service.cancel();
        return;
    }
    // A provider disaster-recovery callback also has ret=0 and nonempty fields.
    // It must not be presented as human verification or submitted as real proof.
    if ((code.isDouble() && code.toDouble() != 0) || ticket.startsWith("trerror_")) {
        respond(peer, 200, "{\"accepted\":false,\"sdkError\":true}");
        emit sdkFailed(code.toInt(-1));
        return;
    }
    if (!ret.isDouble() || ret.toDouble() != 0 || ticket.isEmpty() || randstr.isEmpty() ||
        ticket.size() > 16384 || randstr.size() > 1024 || m_generation != m_service.generation() ||
        !m_service.open()) {
        respond(peer, 400, "{}");
        return;
    }
    const auto generation = m_generation;
    respond(peer, 202, "{\"accepted\":true}");
    stop(); // Revoke the browser capability before calling reentrant business code.
    m_service.submitTencent(generation, ticket, randstr);
    if (generation == m_service.generation() &&
        m_service.state() == VerificationService::State::Verifying)
        emit proofSubmitted();
}
} // namespace EchoMusic
