#pragma once
#include "AccountService.h"
#include "ContentBlacklistModel.h"
#include <array>
#include <memory>
namespace EchoMusic
{
/// One account-scoped owner for both categories, offset pagination and confirmed
/// removal. QML only presents the service, never decodes upstream metadata.
class ContentBlacklistService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(EchoMusic::ContentBlacklistModel *entries READ entries NOTIFY changed)
    Q_PROPERTY(int category READ category NOTIFY changed)
    Q_PROPERTY(bool singer READ singer NOTIFY changed)
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString songCount READ songCount NOTIFY changed)
    Q_PROPERTY(QString singerCount READ singerCount NOTIFY changed)
    Q_PROPERTY(bool pendingRemoval READ pendingRemoval NOTIFY changed)
    Q_PROPERTY(bool pendingSinger READ pendingSinger NOTIFY changed)
    Q_PROPERTY(QString pendingName READ pendingName NOTIFY changed)
    Q_PROPERTY(QString removingKey READ removingKey NOTIFY changed)
    Q_PROPERTY(bool removing READ removing NOTIFY changed)
public:
    ContentBlacklistService(ApiClient &, AccountService &, QObject *parent = nullptr);
    ~ContentBlacklistService() override;
    ContentBlacklistModel *entries()
    {
        return m_buckets[m_category].model.get();
    }
    int category() const
    {
        return m_category;
    }
    bool singer() const
    {
        return m_category == 1;
    }
    bool loaded() const
    {
        return m_buckets[m_category].loaded;
    }
    bool loading() const
    {
        return m_buckets[m_category].request != 0;
    }
    bool hasMore() const
    {
        return more(m_category);
    }
    QString error() const
    {
        return m_buckets[m_category].error;
    }
    QString songCount() const
    {
        return countText(0);
    }
    QString singerCount() const
    {
        return countText(1);
    }
    bool pendingRemoval() const
    {
        return !m_pending.isEmpty();
    }
    bool pendingSinger() const
    {
        return m_pendingCategory == 1;
    }
    QString pendingName() const;
    QString removingKey() const
    {
        return m_submitted.value("key").toString();
    }
    bool removing() const
    {
        return m_write != 0;
    }
    /// 歌曲级菜单状态独立于管理对话框的分类分页；索引按需完整加载。
    enum class SongStatus
    {
        Unknown,
        Present,
        Absent
    };
    Q_ENUM(SongStatus)
    SongStatus songStatus(const QString &hash) const;
    SongStatus singerStatus(quint64 id) const;
    void ensureSingerIndex();
    bool singerBusy() const
    {
        return m_singerWrite != 0 || m_singerIntentId != 0;
    }
    void toggleSinger(quint64 id, const QString &name);
    /// 菜单操作占用（索引加载、待决意图或写入中）；QML 用它禁用菜单项。
    bool menuBusy() const
    {
        return m_menuWrite != 0 || m_menuIntent.active;
    }
    /// 菜单不感兴趣命令；索引未加载时先完成加载，结果经 menuWriteFinished 返回。
    void requestSongBlacklist(const QString &hash, const QString &name, const QString &mixSongId,
                              bool remove);
    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();
    Q_INVOKABLE void selectCategory(int);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void requestRemoval(const QString &key);
    Q_INVOKABLE void cancelRemoval();
    Q_INVOKABLE void confirmRemoval();
    void shutdown();
signals:
    void changed();
    void removalFinished(bool success, const QString &message);
    /// (success, informational, message) → Success / Info / Warning toast。
    void singerWriteFinished(bool success, const QString &message);
    void menuWriteFinished(bool success, bool informational, const QString &message);

private:
    friend class ContentBlacklistTestAccess;
    struct Bucket
    {
        std::unique_ptr<ContentBlacklistModel> model;
        quint64 request = 0;
        int page = 0, requestedPage = 0, pageSize = 30, lastPageCount = 0;
        qint64 total = -1;
        bool loaded = false;
        QString error;
    };
    struct SongIndex
    {
        QHash<QString, QJsonObject> entries;
        quint64 request = 0;
        quint32 requestedPage = 0;
        int pageSize = 500;
        qint64 total = -1;
        int lastPageCount = 0;
        bool loaded = false;
        QString error;
    };
    struct MenuIntent
    {
        bool active = false, remove = false;
        QString key, name, mixSongId;
    };
    bool current() const;
    bool more(int category) const;
    QString countText(int category) const;
    void read(int category, bool refresh);
    void clear();
    void succeeded(quint64, const QByteArray &);
    void failed(quint64, const QByteArray &);
    void ensureSongIndex();
    void advanceSongIndex();
    void continueMenuIntent();
    void beginMenuWrite();
    void finishMenu(bool success, bool informational, const QString &message);
    static QString normalizedKey(const QString &hash);
    ApiClient &m_api;
    AccountService &m_account;
    std::array<Bucket, 2> m_buckets;
    bool m_singerFullRead = false, m_singerRemoving = false;
    quint64 m_singerWrite = 0, m_singerIntentId = 0;
    QString m_singerName;
    void continueSinger();
    SongIndex m_songIndex;
    MenuIntent m_menuIntent;
    quint64 m_menuWrite = 0;
    bool m_menuWriteRemove = false;
    QString m_menuWriteKey;
    quint64 m_revision = 0, m_write = 0, m_activation = 0, m_writeActivation = 0;
    int m_category = 0, m_pendingCategory = 0, m_writeCategory = 0;
    bool m_active = false, m_closing = false, m_realign = false;
    QJsonObject m_pending, m_submitted;
};
} // namespace EchoMusic
