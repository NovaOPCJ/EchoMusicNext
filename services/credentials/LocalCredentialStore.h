#pragma once
#include <QObject>
#include <QThread>

namespace EchoMusic
{
/// Asynchronous application-local SQLite byte records. No OS credential APIs,
/// encryption or QML properties. Completion IDs belong to the caller.
class LocalCredentialStore : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void read(quint64 operation, const QString &key) = 0;
    virtual void write(quint64 operation, const QString &key, const QByteArray &bytes) = 0;
    virtual void remove(quint64 operation, const QString &key) = 0;
    virtual void close(quint64 operation)
    {
        emit finished(operation, true, {});
    }
signals:
    void finished(quint64 operation, bool success, const QByteArray &bytes);
};
class LocalCredentialWorker;
class SqliteCredentialStore final : public LocalCredentialStore
{
    Q_OBJECT
public:
    explicit SqliteCredentialStore(QString path, QObject *parent = nullptr);
    ~SqliteCredentialStore() override;
    void read(quint64 operation, const QString &key) override;
    void write(quint64 operation, const QString &key, const QByteArray &bytes) override;
    void remove(quint64 operation, const QString &key) override;
    void close(quint64 operation) override;

private:
    void dispatch(quint64 operation, int command, QString key = {}, QByteArray bytes = {});
    QThread m_thread;
    LocalCredentialWorker *m_worker;
    bool m_closed = false;
};
} // namespace EchoMusic
