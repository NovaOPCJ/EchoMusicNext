#pragma once
#include "PlaylistMutationService.h"
#include <QAbstractListModel>
#include <QHash>
#include <QtQml/qqmlregistration.h>

namespace EchoMusic
{
/// A temporary editor snapshot, never a second writable library. Full pages must
/// agree on revision/count; fixed rows retain their original slots at commit.
class PlaylistOrderModel final : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Library-owned order editor")
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
    Q_PROPERTY(int selected READ selected WRITE select NOTIFY changed)
    Q_PROPERTY(int tab READ tab NOTIFY changed)
    Q_PROPERTY(int mode READ mode NOTIFY changed)
    Q_PROPERTY(quint64 listId READ listId NOTIFY changed)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
public:
    enum Role
    {
        Title = Qt::UserRole + 1,
        Subtitle
    };
    PlaylistOrderModel(ApiClient &api, PlaylistMutationService &mutations,
                       QObject *parent = nullptr);
    ~PlaylistOrderModel() override;
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_order.size();
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override
    {
        return {{Title, "title"}, {Subtitle, "subtitle"}};
    }
    bool busy() const
    {
        return m_request || m_saving;
    }
    bool saving() const
    {
        return m_saving;
    }
    bool dirty() const
    {
        return m_order != m_original;
    }
    int selected() const
    {
        return m_selected;
    }
    int tab() const
    {
        return m_tab;
    }
    int mode() const
    {
        return m_mode;
    }
    quint64 listId() const
    {
        return m_listId;
    }
    QString trackTitle() const
    {
        return m_trackTitle;
    }
    QString errorMessage() const
    {
        return m_error;
    }
    Q_INVOKABLE void open(int tab);
    /// Tracks mode edits one owned cloud playlist. SortTracks writes only after
    /// a fresh revision and count read.
    Q_INVOKABLE void openTracks(quint64 listId, const QString &title);
    Q_INVOKABLE void close();
    Q_INVOKABLE QString titleAt(int row) const
    {
        return data(index(row), Title).toString();
    }
    Q_INVOKABLE void select(int row);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void restore();
    Q_INVOKABLE void save();
signals:
    void changed();
    void saved();
    /// Emit success feedback for both playlist and track ordering modes.
    void notice(const QString &message, bool warning);
    void invalidated();

private:
    friend class PlaylistOrderModelTestAccess;
    QList<ApiClient::PlaylistPosition> positions() const;
    QList<ApiClient::PlaylistTrackPosition> trackPositions() const;
    void succeedTracks(const QJsonObject &object);
    struct Row
    {
        quint64 id;
        int type;
        qint64 sort;
        QString title, subtitle;
        quint64 songs;
        bool fixed = false, hidden = false;
    };
    void succeed(quint64 id, const QByteArray &payload);
    void fail(const QString &message);
    ApiClient &m_api;
    PlaylistMutationService &m_mutations;
    QList<Row> m_snapshot;
    QList<int> m_order, m_original;
    quint64 m_request = 0, m_session = 0, m_version = 0, m_total = 0;
    quint64 m_trackRevision = 0, m_listId = 0;
    QHash<quint64, bool> m_seenFiles;
    quint32 m_page = 1;
    int m_tab = 0, m_selected = -1, m_mode = 0;
    bool m_open = false, m_checking = false, m_saving = false;
    QString m_error, m_trackTitle;
};
} // namespace EchoMusic
