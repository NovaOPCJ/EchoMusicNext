#pragma once
// curl.h includes Windows headers before callers can suppress min/max macros.
#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include "HttpRequestBody.h"
#include "NetworkConfiguration.h"
#include <QObject>
#include <QPointer>
#include <QTcpSocket>
#include <QSocketNotifier>
#include <QTimer>
#include <QHash>
#include <curl/curl.h>

namespace EchoMusic
{
class ProxySocketBridge;
/// One absolute-form HTTP transaction or opaque CONNECT tunnel. Qt owns routing/TLS/socket
/// lifecycle; libcurl's nonblocking public multi API handles explicit proxy credentials. No OS
/// credential lookup, cookie engine or origin auth is enabled.
class HttpProxyRequest final : public QObject
{
public:
    /// Called by pre-Qt bootstrap before any threads on older system libcurl.
    static bool initializeRuntime();
    HttpProxyRequest(QTcpSocket *client, QTcpSocket *upstream, const NetworkProxyEndpoint &endpoint,
                     const QUrl &target, const QByteArray &header, const QByteArray &initialBody,
                     const QString &username, const QString &password, QObject *parent);
    ~HttpProxyRequest() override;
    void cancel();

private:
    struct Watch
    {
        QSocketNotifier *read = nullptr, *write = nullptr;
    };
    void readBody();
    void start();
    void reject(int status);
    void cleanup();
    void action(curl_socket_t socket, int events);
    void messages();
    void watch(curl_socket_t socket, int events);
    void resume();
    void establishTunnel();
    void serviceTunnel();
    size_t header(const char *bytes, size_t size);
    size_t receive(const char *bytes, size_t size);
    void writeHeaders();
    static int socketCallback(CURL *, curl_socket_t socket, int events, void *owner, void *);
    static int timerCallback(CURLM *, long timeout, void *owner);
    static size_t headerCallback(char *bytes, size_t unit, size_t count, void *owner);
    static size_t receiveCallback(char *bytes, size_t unit, size_t count, void *owner);
    static size_t readCallback(char *bytes, size_t unit, size_t count, void *owner);
    static int seekCallback(void *owner, curl_off_t offset, int origin);
    static int beforeRequest(void *owner, char *, char *, int, int);
    static int trailersCallback(curl_slist **headers, void *owner);
    QPointer<QTcpSocket> m_client;
    ProxySocketBridge *m_bridge = nullptr;
    HttpRequestBody m_body;
    CURL *m_easy = nullptr;
    CURLM *m_multi = nullptr;
    curl_slist *m_requestHeaders = nullptr;
    QHash<curl_socket_t, Watch> m_watches;
    QTimer m_timer;
    QUrl m_target;
    HttpRequestBody::Headers m_headers, m_responseHeaders;
    QByteArray m_method, m_username, m_password;
    qsizetype m_responseHeaderBytes = 0;
    int m_status = 0, m_authResponses = 0, m_failure = 0;
    long m_requestSizeAtChallenge = 0;
    qint64 m_readOffset = 0;
    curl_socket_t m_tunnelSocket = CURL_SOCKET_BAD;
    QByteArray m_tunnelData;
    bool m_headersSent = false, m_closed = false, m_chunked = false, m_processing = false,
         m_paused = false, m_resumeQueued = false, m_rewindAllowed = false, m_started = false,
         m_connect = false, m_tunnel = false, m_uploadPaused = false;
};
} // namespace EchoMusic
