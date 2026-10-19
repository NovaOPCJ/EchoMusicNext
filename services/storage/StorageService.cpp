#include "StorageService.h"
#include "DatabaseWorker.h"

namespace EchoMusic
{
StorageService::StorageService(QObject *parent) : QObject(parent), m_worker(new DatabaseWorker)
{
    qRegisterMetaType<StoredAccount>();
    qRegisterMetaType<StoredAccounts>();
    qRegisterMetaType<StoredPlaylistCover>();
    qRegisterMetaType<StoredPlaylistCovers>();
    m_worker->moveToThread(&m_thread);
    connect(this, &StorageService::initializeRequested, m_worker, &DatabaseWorker::initialize);
    connect(m_worker, &DatabaseWorker::initialized, this, [this] {
        m_state = State::Ready;
        emit initialized();
    });
    connect(m_worker, &DatabaseWorker::failed, this, [this](const QString &message) {
        if (m_state == State::Initializing)
            m_state = State::Error;
        emit failed(message);
    });
    connect(this, &StorageService::recordSearchRequested, m_worker, &DatabaseWorker::recordSearch);
    connect(this, &StorageService::removeSearchRequested, m_worker, &DatabaseWorker::removeSearch);
    connect(this, &StorageService::clearSearchesRequested, m_worker,
            &DatabaseWorker::clearSearches);
    connect(this, &StorageService::saveAccountRequested, m_worker, &DatabaseWorker::saveAccount);
    connect(this, &StorageService::updateAccountIdentityRequested, m_worker,
            &DatabaseWorker::updateAccountIdentity);
    connect(this, &StorageService::selectAccountRequested, m_worker,
            &DatabaseWorker::selectAccount);
    connect(this, &StorageService::removeAccountRequested, m_worker,
            &DatabaseWorker::removeAccount);
    connect(m_worker, &DatabaseWorker::accountsReady, this, &StorageService::accountsReady);
    connect(m_worker, &DatabaseWorker::accountsFailed, this, &StorageService::accountsFailed);
    connect(this, &StorageService::closeRequested, m_worker, &DatabaseWorker::close);
    connect(m_worker, &DatabaseWorker::historyReady, this, [this](const QStringList &queries) {
        if (m_searchHistory == queries)
            return;
        m_searchHistory = queries;
        emit searchHistoryChanged();
    });
    connect(m_worker, &DatabaseWorker::historyFailed, this, &StorageService::historyFailed);
    connect(this, &StorageService::loadPlaylistCoversRequested, m_worker,
            &DatabaseWorker::loadPlaylistCovers);
    connect(this, &StorageService::savePlaylistCoverRequested, m_worker,
            &DatabaseWorker::savePlaylistCover);
    connect(m_worker, &DatabaseWorker::playlistCoversReady, this,
            &StorageService::playlistCoversReady);
    connect(m_worker, &DatabaseWorker::playlistCoversFailed, this,
            &StorageService::playlistCoversFailed);
    // QThread::quit is thread-safe. Direct delivery permits a destructor join
    // without GUI event delivery, while the queued close drains earlier writes.
    connect(m_worker, &DatabaseWorker::drained, &m_thread, &QThread::quit, Qt::DirectConnection);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(&m_thread, &QThread::finished, this, &StorageService::closed);
    m_thread.setObjectName(QStringLiteral("echo-storage"));
    m_thread.start();
}
StorageService::~StorageService()
{
    shutdown();
    // Normal application shutdown waits via the closed signal before quitting.
    // This fallback only joins the worker; it never waits for GUI-bound replies.
    m_thread.wait();
}
void StorageService::initialize(const QString &path)
{
    if (m_closing || m_state == State::Initializing || m_state == State::Ready)
        return;
    m_state = State::Initializing;
    emit initializeRequested(path);
}
void StorageService::loadPlaylistCovers(quint64 request, quint64 account)
{
    if (!m_closing && account)
        emit loadPlaylistCoversRequested(request, account);
}
void StorageService::savePlaylistCover(quint64 account, const StoredPlaylistCover &cover)
{
    if (!m_closing && account && cover.listId)
        emit savePlaylistCoverRequested(account, cover);
}
void StorageService::shutdown()
{
    if (m_closing)
        return;
    m_closing = true;
    emit closeRequested();
}
void StorageService::recordSearch(const QString &query)
{
    if (!m_closing && !query.trimmed().isEmpty())
        emit recordSearchRequested(query.trimmed());
}
void StorageService::removeSearch(const QString &query)
{
    if (!m_closing)
        emit removeSearchRequested(query);
}
void StorageService::clearSearches()
{
    if (!m_closing)
        emit clearSearchesRequested();
}
void StorageService::updateAccountIdentity(quint64 userId, const QString &kind)
{
    if (!m_closing)
        emit updateAccountIdentityRequested(userId, kind);
}
void StorageService::saveAccount(quint64 operation, const StoredAccount &account, bool active)
{
    if (!m_closing)
        emit saveAccountRequested(operation, account, active);
}
void StorageService::selectAccount(quint64 operation, quint64 userId)
{
    if (!m_closing)
        emit selectAccountRequested(operation, userId);
}
void StorageService::removeAccount(quint64 operation, quint64 userId)
{
    if (!m_closing)
        emit removeAccountRequested(operation, userId);
}
} // namespace EchoMusic
