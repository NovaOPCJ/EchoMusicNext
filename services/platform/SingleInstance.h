#pragma once
#include <QObject>
#include <QUrl>
#include <QLocalServer>
#include <QLockFile>
#include <memory>

namespace EchoMusic
{
/// One application process per user data directory. Acquire before opening
/// preferences, SQL, Rust runtimes or log files. Secondary launches only activate.
class SingleInstance final : public QObject
{
    Q_OBJECT
public:
    enum class Result
    {
        Primary,
        Activated,
        Failed
    };
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;
    Result start(const QString &dataDirectory, int timeoutMs = 5000, const QUrl &openUrl = {});
    QString errorString() const
    {
        return m_error;
    }
    QString lockFilePath() const
    {
        return m_lockPath;
    }

private:
    bool activate(const QString &serverName, int timeoutMs, const QByteArray &payload);
    void acceptConnections();
    QLocalServer m_server;
    std::unique_ptr<QLockFile> m_lock;
    QString m_error;
    QString m_lockPath;
signals:
    void activationRequested();
    void urlRequested(const QUrl &url);
};
} // namespace EchoMusic
