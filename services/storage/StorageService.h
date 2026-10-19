#pragma once
#include <QObject>
#include <QThread>
#include <QStringList>
#include "StoredAccount.h"
#include "StoredPlaylistCover.h"

namespace EchoMusic
{
class DatabaseWorker;
/// App service owns the storage thread. Shutdown is asynchronous while UI is live.
class StorageService final : public QObject
{
    Q_OBJECT
public:
    explicit StorageService(QObject *parent = nullptr);
    ~StorageService() override;
    enum class State
    {
        Idle,
        Initializing,
        Ready,
        Error
    };
    State state() const
    {
        return m_state;
    }
    void initialize(const QString &path);
    void shutdown();
    QStringList searchHistory() const
    {
        return m_searchHistory;
    }
    void recordSearch(const QString &query);
    void removeSearch(const QString &query);
    void clearSearches();
    void loadPlaylistCovers(quint64 request, quint64 account);
    void savePlaylistCover(quint64 account, const StoredPlaylistCover &cover);
    void saveAccount(quint64 operation, const StoredAccount &account, bool active);
    void updateAccountIdentity(quint64 userId, const QString &kind);
    void selectAccount(quint64 operation, quint64 userId);
    void removeAccount(quint64 operation, quint64 userId);
signals:
    void initialized();
    void failed(const QString &message);
    void closed();
    void initializeRequested(const QString &path);
    void recordSearchRequested(const QString &query);
    void removeSearchRequested(const QString &query);
    void clearSearchesRequested();
    void closeRequested();
    void searchHistoryChanged();
    void historyFailed(const QString &message);
    void playlistCoversReady(quint64 request, quint64 account,
                             const EchoMusic::StoredPlaylistCovers &covers);
    void playlistCoversFailed(const QString &message);
    void loadPlaylistCoversRequested(quint64 request, quint64 account);
    void savePlaylistCoverRequested(quint64 account, const EchoMusic::StoredPlaylistCover &cover);
    void accountsReady(quint64 operation, const EchoMusic::StoredAccounts &accounts,
                       quint64 activeUserId);
    void accountsFailed(quint64 operation);
    void saveAccountRequested(quint64 operation, const EchoMusic::StoredAccount &account,
                              bool active);
    void updateAccountIdentityRequested(quint64 userId, const QString &kind);
    void selectAccountRequested(quint64 operation, quint64 userId);
    void removeAccountRequested(quint64 operation, quint64 userId);

private:
    QThread m_thread;
    DatabaseWorker *m_worker;
    QStringList m_searchHistory;
    State m_state = State::Idle;
    bool m_closing = false;
};
} // namespace EchoMusic
