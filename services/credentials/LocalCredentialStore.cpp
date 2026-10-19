#include "LocalCredentialStore.h"
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace EchoMusic
{
class LocalCredentialWorker final : public QObject
{
    Q_OBJECT
public:
    explicit LocalCredentialWorker(QString path) : m_path(std::move(path)) {}
    ~LocalCredentialWorker() override
    {
        closeDatabase();
    }
    void execute(quint64 operation, int command, const QString &key, const QByteArray &bytes)
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (command == 3) {
            closeDatabase();
            emit finished(operation, true, {});
            return;
        }
        if (!open()) {
            emit finished(operation, false, {});
            return;
        }
        QSqlQuery query(m_database);
        bool ok = false;
        QByteArray result;
        if (command == 0) {
            query.prepare("SELECT value FROM credentials WHERE key=?");
            query.addBindValue(key);
            ok = query.exec() && query.next();
            if (ok)
                result = query.value(0).toByteArray();
        } else if (command == 1) {
            query.prepare("INSERT INTO credentials(key,value) VALUES(?,?) ON CONFLICT(key) DO "
                          "UPDATE SET value=excluded.value");
            query.addBindValue(key);
            query.addBindValue(bytes.isNull() ? QByteArray("") : bytes);
            ok = query.exec();
        } else {
            query.prepare("DELETE FROM credentials WHERE key=?");
            query.addBindValue(key);
            ok = query.exec();
        }
        emit finished(operation, ok, result);
    }
signals:
    void finished(quint64 operation, bool success, const QByteArray &bytes);

private:
    bool open()
    {
        if (m_database.isOpen())
            return true;
        if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
            return false;
        if (!m_database.isValid()) {
            m_database = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
            m_database.setDatabaseName(m_path);
            m_database.setConnectOptions("QSQLITE_BUSY_TIMEOUT=5000");
        }
        if (!m_database.open())
            return false;
        QSqlQuery query(m_database);
        const bool ok = query.exec("PRAGMA journal_mode=WAL") &&
                        query.exec("PRAGMA synchronous=NORMAL") &&
                        query.exec("CREATE TABLE IF NOT EXISTS credentials(key TEXT PRIMARY KEY "
                                   "NOT NULL,value BLOB NOT NULL)");
        if (!ok)
            m_database.close();
        return ok;
    }
    void closeDatabase()
    {
        const auto name = m_database.connectionName();
        m_database.close();
        m_database = {};
        if (!name.isEmpty())
            QSqlDatabase::removeDatabase(name);
    }
    QString m_path;
    QSqlDatabase m_database;
};
SqliteCredentialStore::SqliteCredentialStore(QString path, QObject *parent)
    : LocalCredentialStore(parent), m_worker(new LocalCredentialWorker(std::move(path)))
{
    m_worker->moveToThread(&m_thread);
    connect(m_worker, &LocalCredentialWorker::finished, this, &LocalCredentialStore::finished);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.setObjectName("echo-local-credentials");
    m_thread.start();
}
SqliteCredentialStore::~SqliteCredentialStore()
{
    // Queue a barrier after all submitted writes; no GUI reply is needed to join.
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker] { worker->execute(0, 3, {}, {}); },
        Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
}
void SqliteCredentialStore::dispatch(quint64 operation, int command, QString key, QByteArray bytes)
{
    if (m_closed) {
        QMetaObject::invokeMethod(
            this, [this, operation] { emit finished(operation, false, {}); }, Qt::QueuedConnection);
        return;
    }
    QMetaObject::invokeMethod(
        m_worker, [worker = m_worker, operation, command, key = std::move(key),
                   bytes = std::move(bytes)] { worker->execute(operation, command, key, bytes); });
}
void SqliteCredentialStore::read(quint64 operation, const QString &key)
{
    dispatch(operation, 0, key);
}
void SqliteCredentialStore::write(quint64 operation, const QString &key, const QByteArray &bytes)
{
    dispatch(operation, 1, key, bytes);
}
void SqliteCredentialStore::remove(quint64 operation, const QString &key)
{
    dispatch(operation, 2, key);
}
void SqliteCredentialStore::close(quint64 operation)
{
    if (m_closed) {
        QMetaObject::invokeMethod(
            this, [this, operation] { emit finished(operation, true, {}); }, Qt::QueuedConnection);
        return;
    }
    dispatch(operation, 3);
    m_closed = true;
}
} // namespace EchoMusic
#include "LocalCredentialStore.moc"
