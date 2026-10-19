#pragma once
#include <QObject>
#include <QTimer>
#include <utility>
#include "ApiClient.h"
namespace EchoMusic
{
/// Owns QR key, polling and GUI state. The key never becomes a QML property.
/// ApiClient must outlive this service; restarting invalidates old request IDs.
class QrLoginService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY changed)
    Q_PROPERTY(QString imageDataUrl READ imageDataUrl NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
public:
    enum class State
    {
        Idle,
        Creating,
        Waiting,
        Scanned,
        Authorizing,
        Expired,
        Error
    };
    Q_ENUM(State)
    explicit QrLoginService(ApiClient &api, QObject *parent = nullptr);
    ~QrLoginService() override;
    State state() const
    {
        return m_state;
    }
    QString imageDataUrl() const
    {
        return m_image;
    }
    QString errorMessage() const
    {
        return m_error;
    }
    void setProvider(ApiClient::QrProvider provider);
    /// Leaving a method invalidates its entire QR flow. Returning creates a new
    /// code, rather than showing an expired cached session or accepting late work.
    void pause();
    void resume();
    ApiClient::QrProvider provider() const
    {
        return m_provider;
    }
    Q_INVOKABLE void start();
    Q_INVOKABLE void cancel();
    QByteArray takeAuthorization()
    {
        return std::exchange(m_authorization, {});
    }
signals:
    void changed();
    /// Canonical QR authorization for AccountService; deliberately not a QML property.
    void authorized();

private:
    friend class QrLoginServiceTestAccess;
    void clear();
    void poll();
    void succeed(quint64 id, const QByteArray &payload);
    void fail(quint64 id, const QByteArray &payload);
    ApiClient &m_api;
    QTimer m_timer;
    State m_state = State::Idle;
    QString m_key, m_image, m_error;
    QByteArray m_authorization;
    QJsonObject m_providerSession;
    ApiClient::QrProvider m_provider = ApiClient::QrProvider::Kugou;
    bool m_paused = false;
    quint64 m_create = 0, m_check = 0, m_exchange = 0, m_finish = 0, m_revision = 0;
};
} // namespace EchoMusic
