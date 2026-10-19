#pragma once
#include "ApiClient.h"
#include <QSet>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Single account-scoped collection writer; detail pages only read its state.
class VideoCollectionService final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by DetailsService")
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
public:
    VideoCollectionService(ApiClient &, bool network, QObject *parent);
    ~VideoCollectionService() override;
    bool loaded() const
    {
        return m_loaded;
    }
    bool loading() const
    {
        return m_read != 0;
    }
    QString errorMessage() const
    {
        return m_error;
    }
    Q_INVOKABLE bool contains(const QString &id) const
    {
        return m_ids.contains(id);
    }
    Q_INVOKABLE bool pending(const QString &id) const;
    Q_INVOKABLE void ensureLoaded();
    Q_INVOKABLE void toggle(const QString &id);
    void shutdown();
signals:
    void changed();
    void notice(const QString &, bool);
    void mutated(const QString &id, bool collected);

private:
    friend class DetailsTestAccess;
    void read();
    void reset();
    void accept(quint64, const QByteArray &);
    void fail(quint64, const QByteArray &);
    ApiClient &m_api;
    QSet<QString> m_ids, m_draft;
    QHash<quint64, QPair<QString, bool>> m_writes;
    quint64 m_read = 0;
    quint32 m_page = 1;
    bool m_network, m_closed = false, m_loaded = false;
    QString m_error;
};
} // namespace EchoMusic
