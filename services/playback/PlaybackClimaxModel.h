#pragma once
#include "ApiClient.h"
#include "PlaybackTrack.h"
#include <QAbstractListModel>
namespace EchoMusic
{
/// One GUI-thread, current-committed-track metadata model. Optional failures
/// leave playback untouched. Rust alone parses upstream fields/units.
class PlaybackClimaxModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    enum Role
    {
        Start = Qt::UserRole + 1,
        End
    };
    explicit PlaybackClimaxModel(ApiClient &api, QObject *parent = nullptr);
    ~PlaybackClimaxModel() override;
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : int(m_marks.size());
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override
    {
        return {{Start, "start"}, {End, "end"}};
    }
    void load(const PlaybackTrack &track, qint64 durationMs);
    void setDuration(qint64 durationMs);
    void clear();
signals:
    void changed();

private:
    friend class PlaybackClimaxModelTestAccess;
    void succeed(quint64 id, const QByteArray &bytes);
    void rebuild();
    ApiClient &m_api;
    quint64 m_request = 0, m_revision = 0;
    qint64 m_duration = 0;
    struct Interval
    {
        qint64 start, end;
    };
    struct Mark
    {
        double start, end;
        bool operator==(const Mark &) const = default;
    };
    QList<Interval> m_intervals;
    QList<Mark> m_marks;
};
} // namespace EchoMusic
