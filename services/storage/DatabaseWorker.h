#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QStringList>
#include "StoredAccount.h"
#include "StoredPlaylistCover.h"

namespace EchoMusic
{
/// Moved to the storage thread before use; its SQL connection is created and
/// destroyed there. No connection or query crosses the thread boundary.
class DatabaseWorker final : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseWorker(QObject *parent = nullptr) : QObject(parent) {}
    ~DatabaseWorker() override;
public slots:
    void initialize(const QString &path);
    void recordSearch(const QString &query);
    void removeSearch(const QString &query);
    void clearSearches();
    void loadPlaylistCovers(quint64 request, quint64 account);
    void savePlaylistCover(quint64 account, const EchoMusic::StoredPlaylistCover &cover);
    void saveAccount(quint64 operation, const EchoMusic::StoredAccount &account, bool active);
    void updateAccountIdentity(quint64 userId, const QString &kind);
    void selectAccount(quint64 operation, quint64 userId);
    void removeAccount(quint64 operation, quint64 userId);
    void close();
signals:
    void initialized();
    void failed(const QString &message);
    void historyReady(const QStringList &queries);
    void historyFailed(const QString &message);
    void playlistCoversReady(quint64 request, quint64 account,
                             const EchoMusic::StoredPlaylistCovers &covers);
    void playlistCoversFailed(const QString &message);
    void drained();
    void accountsReady(quint64 operation, const EchoMusic::StoredAccounts &accounts,
                       quint64 activeUserId);
    void accountsFailed(quint64 operation);

private:
    void publishHistory();
    void publishAccounts(quint64 operation = 0);
    bool readAccounts(StoredAccounts &accounts, quint64 &active);
    void commitAccounts(quint64 operation, bool statementsSucceeded);
    QSqlDatabase m_database;
};
} // namespace EchoMusic
