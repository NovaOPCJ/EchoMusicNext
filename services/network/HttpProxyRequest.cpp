#include "HttpProxyRequest.h"
#include "ProxySocketBridge.h"
#include <QHttpHeaders>
#include <QLoggingCategory>
#include <QSet>
#include <QScopedValueRollback>
#include <array>
#include <algorithm>
#include <limits>

namespace EchoMusic
{
namespace
{
constexpr qint64 bufferLimit = 128 * 1024;
constexpr qsizetype headerLimit = 64 * 1024;
Q_LOGGING_CATEGORY(httpProxyLog, "echo.network.http-proxy")
struct CurlLifetime
{
    CURLcode status = curl_global_init(CURL_GLOBAL_DEFAULT);
    ~CurlLifetime()
    {
        if (status == CURLE_OK)
            curl_global_cleanup();
    }
};
QSet<QByteArray> hopHeaders(const HttpRequestBody::Headers &headers)
{
    QSet<QByteArray> result{
        "connection",        "proxy-connection",  "keep-alive", "te",
        "trailer",           "transfer-encoding", "upgrade",    "proxy-authorization",
        "proxy-authenticate"};
    for (const auto &[name, value] : headers)
        if (name.toLower() == "connection" || name.toLower() == "proxy-connection")
            for (const auto &token : value.split(','))
                result.insert(token.trimmed().toLower());
    return result;
}
} // namespace
HttpProxyRequest::HttpProxyRequest(QTcpSocket *client, QTcpSocket *upstream,
                                   const NetworkProxyEndpoint &endpoint, const QUrl &target,
                                   const QByteArray &rawHeader, const QByteArray &initialBody,
                                   const QString &username, const QString &password,
                                   QObject *parent)
    : QObject(parent), m_client(client), m_target(target), m_username(username.toUtf8()),
      m_password(password.toUtf8())
{
    m_bridge = new ProxySocketBridge(endpoint.address, [this] { reject(502); }, this, upstream);
    connect(client, &QTcpSocket::readyRead, this, [this] { readBody(); });
    connect(client, &QTcpSocket::bytesWritten, this, [this] { resume(); });
    connect(client, &QTcpSocket::disconnected, this, [this] { cancel(); });
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] { action(CURL_SOCKET_TIMEOUT, 0); });
    if (!m_bridge->port()) {
        reject(502);
        return;
    }
    const auto lines = rawHeader.split('\n');
    m_method = lines.first().trimmed().split(' ').first();
    m_connect = m_method == "CONNECT";
    QHttpHeaders validated;
    for (const auto c : m_method)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              QByteArray("!#$%&'*+-.^_`|~").contains(c))) {
            reject(400);
            return;
        }
    for (qsizetype i = 1; i < lines.size(); ++i) {
        auto line = lines[i];
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.isEmpty())
            continue;
        const auto colon = line.indexOf(':');
        if (colon <= 0 || line.startsWith(' ') || line.startsWith('\t') ||
            !validated.append(line.left(colon), line.mid(colon + 1).trimmed())) {
            reject(400);
            return;
        }
        m_headers.append({line.left(colon).toLower(), line.mid(colon + 1).trimmed()});
    }
    if (!m_body.begin(m_headers)) {
        reject(m_body.errorStatus());
        return;
    }
    if (m_connect && !m_body.complete()) {
        reject(400);
        return;
    }
    for (const auto &[name, value] : m_headers)
        if (name == "expect") {
            if (value.toLower() != "100-continue") {
                reject(417);
                return;
            }
            if (!m_body.complete())
                client->write("HTTP/1.1 100 Continue\r\n\r\n");
        }
    if (m_connect)
        m_tunnelData = initialBody;
    else if (!m_body.append(initialBody)) {
        reject(m_body.errorStatus());
        return;
    }
    readBody();
}
HttpProxyRequest::~HttpProxyRequest()
{
    cancel();
    cleanup();
    m_body.clear();
}
void HttpProxyRequest::cleanup()
{
    if (m_processing)
        return;
    m_timer.stop();
    for (const auto &watch : std::as_const(m_watches)) {
        for (auto *notifier : {watch.read, watch.write})
            if (notifier) {
                notifier->setEnabled(false);
                notifier->deleteLater();
            }
    }
    m_watches.clear();
    if (m_multi && m_easy)
        curl_multi_remove_handle(m_multi, m_easy);
    if (m_easy) {
        curl_easy_cleanup(m_easy);
        m_easy = nullptr;
    }
    if (m_multi) {
        curl_multi_cleanup(m_multi);
        m_multi = nullptr;
    }
    curl_slist_free_all(m_requestHeaders);
    m_requestHeaders = nullptr;
}
void HttpProxyRequest::cancel()
{
    m_closed = true;
    m_timer.stop();
    if (m_bridge)
        m_bridge->close();
    if (!m_processing)
        cleanup();
    else
        QTimer::singleShot(0, this, [this] { cleanup(); });
}
void HttpProxyRequest::reject(int status)
{
    if (m_closed)
        return;
    const bool sent = m_headersSent;
    cancel();
    if (!m_client || !m_client->isOpen())
        return;
    if (!sent)
        m_client->write("HTTP/1.1 " + QByteArray::number(status) +
                        " Proxy request failed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    m_client->disconnectFromHost();
}
void HttpProxyRequest::readBody()
{
    if (m_tunnel) {
        serviceTunnel();
        return;
    }
    if (m_closed || !m_client)
        return;
    if (m_connect) {
        if (!m_easy)
            start();
        return;
    }
    if (m_client->bytesAvailable() && !m_body.append(m_client->read(bufferLimit))) {
        reject(m_body.errorStatus());
        return;
    }
    if (!m_easy)
        start();
    else
        resume();
    if (m_client->bytesAvailable())
        QTimer::singleShot(0, this, [this] { readBody(); });
}
bool HttpProxyRequest::initializeRuntime()
{
    static const CurlLifetime lifetime;
    return lifetime.status == CURLE_OK;
}
void HttpProxyRequest::start()
{
    if (!initializeRuntime()) {
        reject(502);
        return;
    }
    const auto *version = curl_version_info(CURLVERSION_NOW);
    if (!version || !(version->features & CURL_VERSION_NTLM)) {
        qCWarning(httpProxyLog) << "libcurl is missing required NTLM support";
        reject(502);
        return;
    }
    m_easy = curl_easy_init();
    m_multi = curl_multi_init();
    if (!m_easy || !m_multi) {
        reject(507);
        return;
    }
    const auto hops = hopHeaders(m_headers);
    const auto add = [this](const QByteArray &header) {
        auto *next = curl_slist_append(m_requestHeaders, header.constData());
        if (!next) {
            m_failure = 507;
            return;
        }
        m_requestHeaders = next;
    };
    bool encoded = false, contentType = false;
    for (const auto &[name, value] : m_headers) {
        if (m_connect)
            break;
        if (name == "trailer" && m_body.expectedSize() < 0)
            add(name + ": " + value);
        if (hops.contains(name) || name == "expect" || name == "content-length")
            continue;
        add(name + (value.isEmpty() ? ";" : ": " + value));
        encoded |= name == "accept-encoding";
        contentType |= name == "content-type";
    }
    if (!encoded)
        add("Accept-Encoding: identity");
    if (!contentType)
        add("Content-Type:");
    // The local client may already have received 100 Continue; do not introduce
    // another upload wait or a Content-Type guessed by a high-level API.
    add("Expect:");
    const auto set = [this](CURLoption option, auto value) {
        if (curl_easy_setopt(m_easy, option, value) != CURLE_OK)
            m_failure = 502;
    };
    auto target = m_target;
    if (m_connect) {
        // Proxy CONNECT is the transport: libcurl must not start origin TLS.
        target.setPort(m_target.port(443));
        target.setScheme("http");
    }
    const auto url = target.toEncoded();
    const auto proxy = QByteArray("http://127.0.0.1:") + QByteArray::number(m_bridge->port());
    set(CURLOPT_URL, url.constData());
    set(CURLOPT_PROXY, proxy.constData());
    set(CURLOPT_NOPROXY, "");
    set(CURLOPT_PROXYUSERNAME, m_username.constData());
    set(CURLOPT_PROXYPASSWORD, m_password.constData());
    set(CURLOPT_PROXYAUTH, long(CURLAUTH_BASIC | CURLAUTH_DIGEST | CURLAUTH_NTLM));
    set(CURLOPT_HTTPAUTH, long(CURLAUTH_NONE));
    set(CURLOPT_NETRC, long(CURL_NETRC_IGNORED));
    set(CURLOPT_NOSIGNAL, 1L);
    set(CURLOPT_PATH_AS_IS, 1L);
    set(CURLOPT_HTTP_VERSION, long(CURL_HTTP_VERSION_1_1));
    set(CURLOPT_FOLLOWLOCATION, 0L);
    set(CURLOPT_MAXREDIRS, 6L); // Also bounds multi-round authentication.
    set(CURLOPT_FRESH_CONNECT, 1L);
    if (!m_connect)
        set(CURLOPT_FORBID_REUSE, 1L);
    else {
        set(CURLOPT_HTTPPROXYTUNNEL, 1L);
        set(CURLOPT_CONNECT_ONLY, 1L);
    }
    set(CURLOPT_CONNECTTIMEOUT_MS, 30000L);
    set(CURLOPT_HTTP_CONTENT_DECODING, 0L);
    set(CURLOPT_HEADERFUNCTION, &HttpProxyRequest::headerCallback);
    set(CURLOPT_HEADERDATA, this);
    set(CURLOPT_WRITEFUNCTION, &HttpProxyRequest::receiveCallback);
    set(CURLOPT_WRITEDATA, this);
    set(CURLOPT_READFUNCTION, &HttpProxyRequest::readCallback);
    set(CURLOPT_READDATA, this);
    set(CURLOPT_SEEKFUNCTION, &HttpProxyRequest::seekCallback);
    set(CURLOPT_SEEKDATA, this);
    set(CURLOPT_PREREQFUNCTION, &HttpProxyRequest::beforeRequest);
    set(CURLOPT_PREREQDATA, this);
    if (m_body.expectedSize() < 0) {
        set(CURLOPT_TRAILERFUNCTION, &HttpProxyRequest::trailersCallback);
        set(CURLOPT_TRAILERDATA, this);
    }
    const bool hasBody =
        m_body.expectedSize() != 0 ||
        std::any_of(m_headers.cbegin(), m_headers.cend(), [](const auto &entry) {
            return entry.first == "content-length" || entry.first == "transfer-encoding";
        });
    if (hasBody || m_method == "POST" || m_method == "PUT" || m_method == "PATCH") {
        set(CURLOPT_POST, 1L);
        set(CURLOPT_POSTFIELDSIZE_LARGE, curl_off_t(m_body.expectedSize()));
    }
    if (m_method == "HEAD")
        set(CURLOPT_NOBODY, 1L);
    set(CURLOPT_CUSTOMREQUEST, m_method.constData());
    set(CURLOPT_HTTPHEADER, m_requestHeaders);
    if (m_failure) {
        reject(m_failure);
        return;
    }
    curl_multi_setopt(m_multi, CURLMOPT_SOCKETFUNCTION, &HttpProxyRequest::socketCallback);
    curl_multi_setopt(m_multi, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(m_multi, CURLMOPT_TIMERFUNCTION, &HttpProxyRequest::timerCallback);
    curl_multi_setopt(m_multi, CURLMOPT_TIMERDATA, this);
    if (curl_multi_add_handle(m_multi, m_easy) != CURLM_OK) {
        reject(502);
        return;
    }
    QTimer::singleShot(0, this, [this] { action(CURL_SOCKET_TIMEOUT, 0); });
}
int HttpProxyRequest::socketCallback(CURL *, curl_socket_t socket, int events, void *owner, void *)
{
    auto *self = static_cast<HttpProxyRequest *>(owner);
    if (!self->m_closed)
        self->watch(socket, events);
    return 0;
}
int HttpProxyRequest::timerCallback(CURLM *, long timeout, void *owner)
{
    auto *self = static_cast<HttpProxyRequest *>(owner);
    if (self->m_closed || timeout < 0)
        self->m_timer.stop();
    else
        self->m_timer.start(int(qMin<long>(timeout, std::numeric_limits<int>::max())));
    return 0;
}
void HttpProxyRequest::watch(curl_socket_t socket, int events)
{
    if (socket == CURL_SOCKET_BAD)
        return;
    if (events == CURL_POLL_REMOVE) {
        const auto old = m_watches.take(socket);
        for (auto *notifier : {old.read, old.write})
            if (notifier) {
                notifier->setEnabled(false);
                notifier->deleteLater();
            }
        return;
    }
    auto &watch = m_watches[socket];
    if (!watch.read) {
        watch.read = new QSocketNotifier(qintptr(socket), QSocketNotifier::Read, this);
        watch.write = new QSocketNotifier(qintptr(socket), QSocketNotifier::Write, this);
        const auto *read = watch.read;
        const auto *write = watch.write;
        connect(watch.read, &QSocketNotifier::activated, this, [this, socket, read] {
            if (m_watches.value(socket).read == read)
                action(socket, CURL_CSELECT_IN);
        });
        connect(watch.write, &QSocketNotifier::activated, this, [this, socket, write] {
            if (m_watches.value(socket).write == write)
                action(socket, CURL_CSELECT_OUT);
        });
    }
    watch.read->setEnabled(events == CURL_POLL_IN || events == CURL_POLL_INOUT);
    watch.write->setEnabled(events == CURL_POLL_OUT || events == CURL_POLL_INOUT);
}
void HttpProxyRequest::action(curl_socket_t socket, int events)
{
    if (m_tunnel) {
        serviceTunnel();
        return;
    }
    if (m_closed || !m_multi || m_processing)
        return;
    m_processing = true;
    int running = 0;
    const auto result = curl_multi_socket_action(m_multi, socket, events, &running);
    m_processing = false;
    if (m_closed) {
        cleanup();
        return;
    }
    if (result != CURLM_OK || m_failure) {
        reject(m_failure ? m_failure : 502);
        return;
    }
    messages();
}
void HttpProxyRequest::messages()
{
    if (!m_multi || m_closed)
        return;
    int remaining = 0;
    while (const auto *message = curl_multi_info_read(m_multi, &remaining)) {
        if (message->msg != CURLMSG_DONE)
            continue;
        const auto result = message->data.result;
        qCDebug(httpProxyLog) << "HTTP proxy transaction completed" << m_status << int(result);
        if (m_connect) {
            if (result == CURLE_OK && m_status == 200)
                establishTunnel();
            else
                reject(502);
            return;
        }
        if (!m_headersSent) {
            reject(502);
            return;
        }
        if (m_chunked && result == CURLE_OK)
            m_client->write("0\r\n\r\n");
        cancel();
        if (m_client)
            m_client->disconnectFromHost();
        return;
    }
}
void HttpProxyRequest::establishTunnel()
{
    if (!m_client ||
        curl_easy_getinfo(m_easy, CURLINFO_ACTIVESOCKET, &m_tunnelSocket) != CURLE_OK ||
        m_tunnelSocket == CURL_SOCKET_BAD) {
        reject(502);
        return;
    }
    // CONNECT_ONLY keeps this easy handle attached to multi for send/recv.
    // Only the proxy was authenticated; no origin TLS handshake was performed.
    m_timer.stop();
    m_tunnel = true;
    m_headersSent = true;
    m_client->write("HTTP/1.1 200 Connection Established\r\n\r\n");
    watch(m_tunnelSocket, CURL_POLL_IN);
    serviceTunnel();
}
void HttpProxyRequest::serviceTunnel()
{
    if (m_closed || !m_tunnel || !m_easy || !m_client || m_processing)
        return;
    bool failed = false, eof = false;
    {
        QScopedValueRollback processing(m_processing, true);
        if (m_tunnelData.isEmpty() && m_client->bytesAvailable())
            m_tunnelData = m_client->read(bufferLimit);
        if (!m_tunnelData.isEmpty()) {
            size_t written = 0;
            const auto result = curl_easy_send(m_easy, m_tunnelData.constData(),
                                               size_t(m_tunnelData.size()), &written);
            failed = result != CURLE_OK && result != CURLE_AGAIN;
            if (written)
                m_tunnelData.remove(0, qsizetype(written));
        }
        std::array<char, 16 * 1024> buffer;
        // Drain libcurl's own buffered bytes before waiting on the descriptor.
        // Each activation is bounded by the downstream write budget.
        while (!failed && m_client->bytesToWrite() < bufferLimit) {
            size_t received = 0;
            const auto capacity =
                qMin<qint64>(buffer.size(), bufferLimit - m_client->bytesToWrite());
            const auto result = curl_easy_recv(m_easy, buffer.data(), size_t(capacity), &received);
            if (result == CURLE_AGAIN)
                break;
            if (result != CURLE_OK) {
                failed = true;
                break;
            }
            if (!received) {
                eof = true;
                break;
            }
            m_client->write(buffer.data(), qint64(received));
        }
    }
    if (m_closed) {
        cleanup();
        return;
    }
    if (failed) {
        reject(502);
        return;
    }
    if (eof) {
        cancel();
        m_client->disconnectFromHost();
        return;
    }
    const bool read = m_client->bytesToWrite() < bufferLimit;
    const bool write = !m_tunnelData.isEmpty() || m_client->bytesAvailable();
    watch(m_tunnelSocket, read ? (write ? CURL_POLL_INOUT : CURL_POLL_IN)
                               : (write ? CURL_POLL_OUT : CURL_POLL_NONE));
}
size_t HttpProxyRequest::headerCallback(char *bytes, size_t unit, size_t count, void *owner)
{
    return static_cast<HttpProxyRequest *>(owner)->header(bytes, unit * count);
}
size_t HttpProxyRequest::header(const char *bytes, size_t size)
{
    if (m_closed || size > size_t(headerLimit))
        return 0;
    const QByteArray line(bytes, qsizetype(size));
    if (line.startsWith("HTTP/")) {
        m_status = line.split(' ').value(1).toInt();
        m_responseHeaders.clear();
        m_responseHeaderBytes = 0;
    }
    m_responseHeaderBytes += qsizetype(size);
    if (m_responseHeaderBytes > headerLimit)
        return 0;
    if (line == "\r\n") {
        if (m_status == 407) {
            curl_easy_getinfo(m_easy, CURLINFO_REQUEST_SIZE, &m_requestSizeAtChallenge);
            m_rewindAllowed = true;
            if (++m_authResponses > 6)
                return 0;
        } else if (m_status >= 200 && !m_connect)
            writeHeaders();
    } else if (!line.startsWith("HTTP/")) {
        const auto colon = line.indexOf(':');
        QHttpHeaders validated;
        if (colon <= 0 || !validated.append(line.left(colon), line.mid(colon + 1).trimmed()))
            return 0;
        m_responseHeaders.append({line.left(colon), line.mid(colon + 1).trimmed()});
    }
    return size;
}
void HttpProxyRequest::writeHeaders()
{
    if (m_headersSent || !m_client || m_closed)
        return;
    const bool noBody = m_method == "HEAD" || m_status == 204 || m_status == 304;
    bool length = false, chunked = false;
    for (const auto &[name, value] : m_responseHeaders) {
        length |= name.toLower() == "content-length";
        chunked |= name.toLower() == "transfer-encoding" && value.toLower().contains("chunked");
    }
    m_chunked = !noBody && (!length || chunked);
    const auto hops = hopHeaders(m_responseHeaders);
    QByteArray response = "HTTP/1.1 " + QByteArray::number(m_status) + " Proxy response\r\n";
    for (const auto &[name, value] : m_responseHeaders) {
        if (hops.contains(name.toLower()) || (m_chunked && name.toLower() == "content-length"))
            continue;
        response += name + ": " + value + "\r\n";
    }
    if (m_chunked)
        response += "Transfer-Encoding: chunked\r\n";
    m_client->write(response + "Connection: close\r\n\r\n");
    m_headersSent = true;
}
size_t HttpProxyRequest::receiveCallback(char *bytes, size_t unit, size_t count, void *owner)
{
    return static_cast<HttpProxyRequest *>(owner)->receive(bytes, unit * count);
}
size_t HttpProxyRequest::receive(const char *bytes, size_t size)
{
    if (m_closed || !m_client)
        return 0;
    if (m_status == 407 || m_status < 200)
        return size;
    if (m_client->bytesToWrite() + qint64(size) + 32 > bufferLimit) {
        m_paused = true;
        return CURL_WRITEFUNC_PAUSE;
    }
    const QByteArray data(bytes, qsizetype(size));
    m_client->write(m_chunked ? QByteArray::number(data.size(), 16) + "\r\n" + data + "\r\n"
                              : data);
    return size;
}
void HttpProxyRequest::resume()
{
    if (m_tunnel) {
        serviceTunnel();
        return;
    }
    if (m_closed || !m_easy || m_resumeQueued || !m_client)
        return;
    const bool send = m_uploadPaused && (m_readOffset < m_body.size() || m_body.complete());
    const bool receive =
        m_paused && m_client->bytesToWrite() <= bufferLimit - CURL_MAX_WRITE_SIZE - 32;
    if (!send && !receive)
        return;
    m_resumeQueued = true;
    QTimer::singleShot(0, this, [this] {
        m_resumeQueued = false;
        if (m_closed || !m_easy)
            return;
        int pause = CURLPAUSE_CONT;
        if (m_paused && m_client->bytesToWrite() > bufferLimit - CURL_MAX_WRITE_SIZE - 32)
            pause |= CURLPAUSE_RECV;
        else
            m_paused = false;
        if (m_uploadPaused && m_readOffset == m_body.size() && !m_body.complete())
            pause |= CURLPAUSE_SEND;
        else
            m_uploadPaused = false;
        m_processing = true;
        const auto result = curl_easy_pause(m_easy, pause);
        m_processing = false;
        if (m_closed) {
            cleanup();
            return;
        }
        if (result != CURLE_OK) {
            reject(502);
            return;
        }
        messages();
    });
}
size_t HttpProxyRequest::readCallback(char *bytes, size_t unit, size_t count, void *owner)
{
    auto *self = static_cast<HttpProxyRequest *>(owner);
    if (self->m_closed)
        return CURL_READFUNC_ABORT;
    self->m_rewindAllowed = false;
    const auto read = self->m_body.readAt(self->m_readOffset, bytes, qint64(unit * count));
    if (!read && !self->m_body.complete()) {
        self->m_uploadPaused = true;
        return CURL_READFUNC_PAUSE;
    }
    if (read > 0)
        self->m_readOffset += read;
    return read < 0 ? CURL_READFUNC_ABORT : size_t(read);
}
int HttpProxyRequest::seekCallback(void *owner, curl_off_t offset, int origin)
{
    auto *self = static_cast<HttpProxyRequest *>(owner);
    if (!self->m_rewindAllowed || offset != 0 || origin != SEEK_SET)
        return CURL_SEEKFUNC_FAIL;
    self->m_rewindAllowed = false;
    self->m_readOffset = 0;
    return CURL_SEEKFUNC_OK;
}
int HttpProxyRequest::trailersCallback(curl_slist **headers, void *owner)
{
    auto *self = static_cast<HttpProxyRequest *>(owner);
    for (const auto &[name, value] : self->m_body.trailers()) {
        const auto header = name + ": " + value;
        auto *next = curl_slist_append(*headers, header.constData());
        if (!next)
            return CURL_TRAILERFUNC_ABORT;
        *headers = next;
    }
    return CURL_TRAILERFUNC_OK;
}
int HttpProxyRequest::beforeRequest(void *owner, char *, char *, int, int)
{
    auto *self = static_cast<HttpProxyRequest *>(owner);
    long sent = 0;
    curl_easy_getinfo(self->m_easy, CURLINFO_REQUEST_SIZE, &sent);
    // Only a newly received 407 permits another request. A dropped response
    // must not replay even an empty POST, where no upload seek callback occurs.
    if (self->m_closed ||
        (self->m_started && (self->m_status != 407 || sent != self->m_requestSizeAtChallenge)))
        return CURL_PREREQFUNC_ABORT;
    self->m_started = true;
    return CURL_PREREQFUNC_OK;
}
} // namespace EchoMusic
