#pragma once

#include <QObject>
#include <QHash>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>

class QTcpSocket;
namespace EchoMusic
{
class VerificationService;

/// Ephemeral loopback transport for the official browser CAPTCHA. Business
/// identity, proof submission and replay remain owned by VerificationService.
/// start() returns a URL with an in-memory capability in its fragment; never log it.
class BrowserVerificationSession final : public QObject
{
    Q_OBJECT
public:
    explicit BrowserVerificationSession(VerificationService &service, QObject *parent = nullptr);
    ~BrowserVerificationSession() override;
    QUrl start();
    bool active() const;
    void stop();
signals:
    void activeChanged();
    void sdkReady();
    void sdkFailed(int code);
    void proofSubmitted();
    void expired();

private:
    void acceptConnections();
    void read(QTcpSocket *peer);
    void respond(QTcpSocket *peer, int status, const QByteArray &body,
                 const QByteArray &contentType = "application/json");
    VerificationService &m_service;
    QTcpServer m_server;
    QTimer m_expiry;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QByteArray m_nonce, m_origin;
    quint64 m_generation = 0;
};
} // namespace EchoMusic
