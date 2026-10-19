#pragma once
#include "NetworkPreferences.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSettings>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
struct PublishedRelease
{
    QString version, notes;
    QUrl url;
};
/// Bounded, asynchronous release discovery on the same network policy as the
/// rest of the app. An empty published feed is not reported as "up to date".
class AboutService final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Application-owned release service")
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString changelog READ changelog CONSTANT)
    Q_PROPERTY(bool autoCheck READ autoCheck WRITE setAutoCheck NOTIFY changed)
    Q_PROPERTY(bool prerelease READ prerelease WRITE setPrerelease NOTIFY changed)
    Q_PROPERTY(bool silentUpdate READ silentUpdate WRITE setSilentUpdate NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString releaseVersion READ releaseVersion NOTIFY changed)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY changed)
    Q_PROPERTY(ResultState resultState READ resultState NOTIFY changed)
    Q_PROPERTY(bool resultOpen READ resultOpen NOTIFY changed)
public:
    enum ResultState
    {
        NoResult,
        UpdateAvailable,
        AlreadyLatest,
        NoPublishedRelease,
        CheckFailed
    };
    Q_ENUM(ResultState)
    AboutService(QSettings &settings, NetworkPreferences &network, QObject *parent = nullptr);
    ~AboutService() override;
    QString version() const;
    QString changelog() const;
    bool autoCheck() const
    {
        return m_autoCheck;
    }
    bool prerelease() const
    {
        return m_prerelease;
    }
    bool silentUpdate() const
    {
        return m_silent;
    }
    bool checking() const
    {
        return m_reply || m_checking;
    }
    QString statusText() const
    {
        return m_status;
    }
    QString releaseVersion() const
    {
        return m_release.version;
    }
    QString releaseNotes() const
    {
        return m_release.notes;
    }
    ResultState resultState() const
    {
        return m_resultState;
    }
    bool resultOpen() const
    {
        return m_resultOpen;
    }
    Q_INVOKABLE void closeResult()
    {
        m_resultOpen = false;
        emit changed();
    }
    void setAutoCheck(bool enabled);
    void setPrerelease(bool enabled);
    void setSilentUpdate(bool enabled);
    Q_INVOKABLE void check(bool automatic = false);
    Q_INVOKABLE void openRepository();
    Q_INVOKABLE void openRelease();
    static QList<PublishedRelease> parseFeed(const QByteArray &xml, QString *error);
    void shutdown();
signals:
    void changed();
    void notice(const QString &message, bool failure);
    void updateAvailable();

private:
    void fetch(const QUrl &url, bool fallback, quint64 revision, bool automatic);
    QSettings &m_settings;
    NetworkPreferences &m_policy;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    PublishedRelease m_release;
    QString m_status;
    quint64 m_revision = 0;
    bool m_autoCheck, m_prerelease, m_silent, m_checking = false, m_closed = false;
    ResultState m_resultState = NoResult;
    bool m_resultOpen = false;
};
} // namespace EchoMusic
