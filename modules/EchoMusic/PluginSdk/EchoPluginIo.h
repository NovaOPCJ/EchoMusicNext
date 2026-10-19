#pragma once
#include "EchoPluginOperation.h"
#include <QNetworkAccessManager>
#include <QThread>
#include <QUrl>
namespace EchoMusic
{
class EchoPluginContext;
/// Plugin-private SQL values and isolated HTTP client. Database access runs in
/// one serial worker; paths are installation-scoped, not plugin-ID-scoped.
class EchoPluginIo final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by EchoPluginContext")
public:
    EchoPluginIo(EchoPluginContext &scope, QString dataPath);
    ~EchoPluginIo() override;
    Q_INVOKABLE EchoMusic::EchoPluginOperation *get(const QUrl &url);
    Q_INVOKABLE EchoMusic::EchoPluginOperation *
    post(const QUrl &url, const QByteArray &body, const QString &contentType = "application/json");
    Q_INVOKABLE EchoMusic::EchoPluginOperation *readValue(const QString &key);
    Q_INVOKABLE EchoMusic::EchoPluginOperation *writeValue(const QString &key,
                                                           const QVariant &value);
    Q_INVOKABLE EchoMusic::EchoPluginOperation *removeValue(const QString &key);
    void shutdown();
    bool isDrained() const
    {
        return m_drained;
    }
signals:
    void drained();

private:
    EchoPluginOperation *request(const QUrl &url, const QByteArray &body,
                                 const QString &contentType, bool post);
    EchoPluginOperation *storage(const QString &key, const QVariant &value, int command);
    EchoPluginContext &m_scope;
    QString m_dataPath;
    QNetworkAccessManager m_network;
    QThread m_thread;
    QObject *m_worker;
    bool m_closed = false, m_drained = false;
};
} // namespace EchoMusic
