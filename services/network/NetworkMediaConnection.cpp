#include "NetworkMediaConnection.h"
#include <QUrl>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <utility>
namespace EchoMusic
{
namespace
{
constexpr qint64 bufferLimit = 128 * 1024;
struct ByteRange
{
    bool partial = false, invalid = false;
    qint64 start = 0, end = -1;
};
ByteRange byteRange(const QByteArray &header, qint64 length)
{
    ByteRange result{false, false, 0, length - 1};
    static const QRegularExpression syntax("^bytes=(\\d*)-(\\d*)$");
    const auto match = syntax.match(QString::fromLatin1(header));
    // RFC 9110 permits ignoring unsupported/malformed ranges, including multipart.
    if (!match.hasMatch() || (match.captured(1).isEmpty() && match.captured(2).isEmpty()))
        return result;
    result.partial = true;
    bool valid = false;
    if (match.captured(1).isEmpty()) {
        const auto suffix = match.captured(2).toLongLong(&valid);
        result.invalid = !valid || suffix <= 0 || length == 0;
        result.start = qMax<qint64>(0, length - suffix);
    } else {
        result.start = match.captured(1).toLongLong(&valid);
        result.invalid = !valid || result.start >= length;
        if (!match.captured(2).isEmpty()) {
            const auto end = match.captured(2).toLongLong(&valid);
            result.invalid |= !valid || end < result.start;
            result.end = qMin(result.end, end);
        }
    }
    return result;
}
} // namespace
NetworkMediaConnection::NetworkMediaConnection(QTcpSocket *client, QNetworkAccessManager &network,
                                               std::function<QUrl(const QString &)> lookup,
                                               int timeout, QObject *parent,
                                               std::function<void(int)> rejected,
                                               bool rangeFallback)
    : QObject(parent), m_client(client), m_network(network), m_lookup(std::move(lookup)),
      m_rejected(std::move(rejected)), m_timeout(timeout), m_rangeFallback(rangeFallback)
{
    client->setParent(this);
    client->setReadBufferSize(64 * 1024);
    connect(client, &QTcpSocket::readyRead, this, [this] { readRequest(); });
    connect(client, &QTcpSocket::bytesWritten, this, [this] {
        if (!m_closed)
            m_deadline.start(m_timeout > 0 ? m_timeout : 30000);
        forward();
    });
    connect(client, &QTcpSocket::disconnected, this, [this] {
        // deleteLater leaves a turn in which upstream readyRead/finished may
        // arrive. Retire I/O immediately, before the QObject is destroyed.
        m_closed = true;
        if (m_reply) {
            m_reply->disconnect(this);
            m_reply->abort();
        }
        deleteLater();
    });
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] { reject(408); });
    m_deadline.start(10000);
    if (client->bytesAvailable())
        QMetaObject::invokeMethod(client, "readyRead", Qt::QueuedConnection);
}
NetworkMediaConnection::~NetworkMediaConnection()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}
void NetworkMediaConnection::close()
{
    m_closed = true;
    m_deadline.stop();
    m_file.close();
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    m_client->abort();
    deleteLater();
}
void NetworkMediaConnection::reject(int status)
{
    if (m_closed || !m_client->isOpen())
        return;
    if (m_headersSent) {
        // A timed-out response must stay truncated; never inject a second HTTP
        // response into audio bytes that have already reached the decoder.
        close();
        return;
    }
    m_closed = true;
    m_deadline.stop();
    m_client->write("HTTP/1.1 " + QByteArray::number(status) +
                    " Resource unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    m_client->disconnectFromHost();
}
void NetworkMediaConnection::readRequest()
{
    if (m_reply || m_file.isOpen() || m_closed || !m_client->isOpen())
        return;
    m_header += m_client->readAll();
    if (m_header.size() > 64 * 1024) {
        reject(431);
        return;
    }
    if (!m_header.contains("\r\n\r\n"))
        return;
    m_deadline.start(m_timeout > 0 ? m_timeout : 30000);
    const auto lines = m_header.split('\n');
    const auto first = lines.first().trimmed().split(' ');
    if (first.size() != 3 || (first[0] != "GET" && first[0] != "HEAD")) {
        reject(405);
        return;
    }
    const auto path = QUrl::fromEncoded(first[1]).path();
    m_token = path.startsWith('/') ? path.mid(1) : QString();
    const auto url = m_lookup(m_token);
    if (url.isEmpty()) {
        reject(404);
        return;
    }
    m_head = first[0] == "HEAD";
    for (const auto &line : lines) {
        const auto colon = line.indexOf(':');
        const auto key = line.left(colon).trimmed().toLower();
        if (colon > 0 && key == "range")
            m_range = line.mid(colon + 1).trimmed();
        if (colon > 0 && key == "if-range")
            m_ifRange = true;
    }
    if (url.isLocalFile()) {
        openLocal(url, m_range);
        return;
    }
    QNetworkRequest request(url);
    request.setTransferTimeout(m_timeout);
    // This reader replaces FFmpeg's stateless HTTP fetch, not a browser
    // session. Cookies/auth from an old account must not survive in QNAM.
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
    // Preserve the exact range/length representation. Qt's automatic content
    // decoding would otherwise change bytes while forwarding origin headers.
    request.setRawHeader("Accept-Encoding", "identity");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const auto line = lines[i].trimmed();
        const auto colon = line.indexOf(':');
        const auto key = line.left(colon).toLower();
        // No arbitrary origin, credentials, cookies or forwarding headers from
        // another localhost client. FFmpeg needs byte ranges and validators.
        if (colon > 0 && (key == "range" || key == "if-range" || key == "if-none-match" ||
                          key == "if-modified-since"))
            request.setRawHeader(key, line.mid(colon + 1).trimmed());
    }
    m_reply = m_head ? m_network.head(request) : m_network.get(request);
    m_reply->setReadBufferSize(bufferLimit);
    connect(m_reply, &QNetworkReply::metaDataChanged, this, [this] { writeHeaders(); });
    connect(m_reply, &QNetworkReply::readyRead, this, [this] { forward(); });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        if (m_closed)
            return;
        m_finished = true;
        const auto status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (!status || (!m_headersSent && status >= 300 && status < 400 && status != 304)) {
            reject(502);
            return;
        }
        forward();
    });
}
void NetworkMediaConnection::openLocal(const QUrl &source, const QByteArray &range)
{
    m_file.setFileName(source.toLocalFile());
    if (!QFileInfo(m_file).isFile() || !m_file.open(QIODevice::ReadOnly)) {
        reject(404);
        return;
    }
    const qint64 length = m_file.size();
    const auto selected = byteRange(range, length);
    const auto [partial, invalid, start, end] = selected;
    if (invalid) {
        m_client->write("HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */" +
                        QByteArray::number(length) +
                        "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        m_closed = true;
        m_client->disconnectFromHost();
        return;
    }
    if (!m_file.seek(start)) {
        reject(500);
        return;
    }
    m_remaining = qMax<qint64>(0, end - start + 1);
    const auto mime = QMimeDatabase()
                          .mimeTypeForFile(m_file.fileName(), QMimeDatabase::MatchExtension)
                          .name()
                          .toLatin1();
    QByteArray headers = partial ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
    headers += "Content-Type: " + mime + "\r\nContent-Length: " + QByteArray::number(m_remaining) +
               "\r\nAccept-Ranges: bytes\r\ntransferMode.dlna.org: Streaming\r\n";
    if (partial)
        headers += "Content-Range: bytes " + QByteArray::number(start) + '-' +
                   QByteArray::number(end) + '/' + QByteArray::number(length) + "\r\n";
    m_client->write(headers + "Connection: close\r\n\r\n");
    m_headersSent = true;
    if (m_head)
        m_remaining = 0;
    forward();
}
void NetworkMediaConnection::writeHeaders()
{
    if (m_closed || m_headersSent || !m_reply || !m_client->isOpen())
        return;
    auto status = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (!status || (status >= 300 && status < 400 && status != 304))
        return;
    bool validLength = false;
    const auto length = m_reply->rawHeader("Content-Length").toLongLong(&validLength);
    ByteRange selected;
    // If-Range returning 200 means the validator failed: honor the full response.
    // Unknown-length origins cannot honestly advertise a suffix or byte total.
    if (m_rangeFallback && status == 200 && validLength && length >= 0 && !m_ifRange &&
        !m_reply->hasRawHeader("Content-Encoding")) {
        selected = byteRange(m_range, length);
        if (selected.invalid) {
            m_client->write("HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */" +
                            QByteArray::number(length) +
                            "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            m_closed = true;
            m_reply->disconnect(this);
            m_reply->abort();
            m_client->disconnectFromHost();
            return;
        }
        if (selected.partial) {
            status = 206;
            m_emulatedRange = true;
            m_skip = selected.start;
            m_rangeRemaining = selected.end - selected.start + 1;
        }
    }
    m_chunked = !m_emulatedRange && !m_head && status != 204 && status != 304 &&
                (!m_reply->hasRawHeader("Content-Length") ||
                 m_reply->rawHeader("Transfer-Encoding").toLower().contains("chunked"));
    QByteArray header = "HTTP/1.1 " + QByteArray::number(status) + " Media\r\n";
    for (const auto &pair : m_reply->rawHeaderPairs()) {
        const auto key = pair.first.toLower();
        if (key == "connection" || key == "transfer-encoding" || key == "proxy-authenticate" ||
            key == "set-cookie" || key == "location" || (m_chunked && key == "content-length") ||
            (m_emulatedRange &&
             (key == "content-length" || key == "content-range" || key == "accept-ranges")))
            continue;
        header += pair.first + ": " + pair.second + "\r\n";
    }
    if (m_chunked)
        header += "Transfer-Encoding: chunked\r\n";
    if (m_emulatedRange)
        header += "Content-Length: " + QByteArray::number(m_rangeRemaining) +
                  "\r\nContent-Range: bytes " + QByteArray::number(selected.start) + '-' +
                  QByteArray::number(selected.end) + '/' + QByteArray::number(length) +
                  "\r\nAccept-Ranges: bytes\r\n";
    if (m_rangeFallback)
        header += "transferMode.dlna.org: Streaming\r\n";
    m_client->write(header + "Connection: close\r\n\r\n");
    m_headersSent = true;
    if (status >= 400 && m_rejected)
        std::exchange(m_rejected, {})(status);
}
void NetworkMediaConnection::forward()
{
    if (!m_closed && m_file.isOpen() && m_client->isOpen()) {
        const auto capacity = bufferLimit - m_client->bytesToWrite();
        if (capacity > 0 && m_remaining > 0) {
            const auto bytes = m_file.read(qMin(capacity, m_remaining));
            if (bytes.isEmpty()) {
                m_closed = true;
                m_client->abort();
                return;
            }
            m_remaining -= bytes.size();
            m_client->write(bytes);
        }
        if (!m_remaining) {
            m_file.close();
            m_closed = true;
            m_client->disconnectFromHost();
        }
        return;
    }
    if (m_closed || !m_reply || !m_client->isOpen())
        return;
    writeHeaders();
    if (m_closed)
        return;
    if (m_emulatedRange && !m_head) {
        // Discard bounded chunks while QNAM backpressure keeps the origin
        // bounded. Yield to the worker event loop between chunks, even when a
        // device seeks near the end of a very large file.
        if (m_skip > 0) {
            const auto skipped = m_reply->read(qMin(m_skip, bufferLimit));
            m_skip -= skipped.size();
            if (m_skip > 0) {
                if (m_reply->bytesAvailable())
                    QMetaObject::invokeMethod(this, [this] { forward(); }, Qt::QueuedConnection);
                else if (m_finished)
                    reject(502);
                return;
            }
        }
        const auto capacity = bufferLimit - m_client->bytesToWrite();
        if (capacity > 0 && m_rangeRemaining > 0) {
            const auto bytes = m_reply->read(qMin(capacity, m_rangeRemaining));
            m_rangeRemaining -= bytes.size();
            m_client->write(bytes);
        }
        if (!m_rangeRemaining) {
            m_closed = true;
            m_reply->disconnect(this);
            m_reply->abort();
            m_client->disconnectFromHost();
        } else if (m_finished && !m_reply->bytesAvailable())
            reject(502);
        return;
    }
    if (m_headersSent && m_reply->bytesAvailable() > 0 &&
        m_client->bytesToWrite() < bufferLimit - 32) {
        const auto bytes = m_reply->read(bufferLimit - 32 - m_client->bytesToWrite());
        if (!bytes.isEmpty())
            m_client->write(
                m_chunked ? QByteArray::number(bytes.size(), 16) + "\r\n" + bytes + "\r\n" : bytes);
    }
    if (m_finished && !m_reply->bytesAvailable()) {
        // QNAM removes upstream chunk framing. Reframe unknown-length bodies
        // and emit the terminal chunk only on success, so a truncated upstream
        // stream remains an error to FFmpeg rather than an apparently clean EOF.
        if (m_chunked && m_reply->error() == QNetworkReply::NoError)
            m_client->write("0\r\n\r\n");
        m_closed = true;
        m_client->disconnectFromHost();
    }
}
} // namespace EchoMusic
