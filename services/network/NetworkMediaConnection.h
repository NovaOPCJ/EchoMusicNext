#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTcpSocket>
#include <QTimer>
#include <functional>
#include <QPointer>
#include <QFile>
namespace EchoMusic
{
/// Bounded streaming for Qt Multimedia's FFmpeg backend and DLNA receivers.
/// Only host-registered opaque URLs are accepted. Closing/releasing a resource
/// aborts actual HTTP I/O. Renderer-only range fallback preserves origin 200
/// responses with If-Range or unknown/encoded lengths rather than inventing 206.
class NetworkMediaConnection final : public QObject
{
public:
    NetworkMediaConnection(QTcpSocket *client, QNetworkAccessManager &network,
                           std::function<QUrl(const QString &)> lookup, int timeout,
                           QObject *parent, std::function<void(int)> rejected = {},
                           bool rangeFallback = false);
    ~NetworkMediaConnection() override;
    QString token() const
    {
        return m_token;
    }
    void close();

private:
    void readRequest();
    void writeHeaders();
    void forward();
    void reject(int status);
    void openLocal(const QUrl &source, const QByteArray &range);
    QTcpSocket *m_client;
    QNetworkAccessManager &m_network;
    std::function<QUrl(const QString &)> m_lookup;
    std::function<void(int)> m_rejected;
    QPointer<QNetworkReply> m_reply;
    QTimer m_deadline;
    QByteArray m_header;
    QByteArray m_range;
    QString m_token;
    int m_timeout;
    QFile m_file;
    qint64 m_remaining = 0;
    qint64 m_skip = 0, m_rangeRemaining = 0;
    bool m_headersSent = false, m_finished = false, m_closed = false, m_head = false,
         m_chunked = false, m_rangeFallback = false, m_emulatedRange = false, m_ifRange = false;
};
} // namespace EchoMusic
