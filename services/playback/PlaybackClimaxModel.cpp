#include "PlaybackClimaxModel.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>
#include <utility>
namespace EchoMusic
{
PlaybackClimaxModel::PlaybackClimaxModel(ApiClient &api, QObject *parent)
    : QAbstractListModel(parent), m_api(api)
{
    connect(&api, &ApiClient::succeeded, this, &PlaybackClimaxModel::succeed);
    connect(&api, &ApiClient::failed, this, [this](quint64 id, int, const QByteArray &) {
        if (id == m_request) {
            m_request = 0;
            m_intervals.clear();
            rebuild();
        }
    });
    connect(&api, &ApiClient::sessionChanged, this, &PlaybackClimaxModel::clear);
    connect(&api, &ApiClient::closed, this, &PlaybackClimaxModel::clear);
}
PlaybackClimaxModel::~PlaybackClimaxModel()
{
    clear();
}
void PlaybackClimaxModel::clear()
{
    const auto request = std::exchange(m_request, 0);
    if (request)
        m_api.cancel(request);
    m_intervals.clear();
    m_duration = 0;
    rebuild();
}
void PlaybackClimaxModel::load(const PlaybackTrack &track, qint64 durationMs)
{
    clear();
    m_duration = durationMs;
    const auto &hash = track.hash;
    if (hash.size() != 32 || !std::all_of(hash.cbegin(), hash.cend(), [](QChar c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }))
        return;
    m_revision = m_api.sessionRevision();
    m_request = m_api.songClimax(hash);
}
QVariant PlaybackClimaxModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_marks.size())
        return {};
    const auto &mark = m_marks[index.row()];
    return role == Start ? QVariant(mark.start) : role == End ? QVariant(mark.end) : QVariant();
}
void PlaybackClimaxModel::setDuration(qint64 durationMs)
{
    if (m_duration == durationMs)
        return;
    m_duration = durationMs;
    rebuild();
}
void PlaybackClimaxModel::succeed(quint64 id, const QByteArray &bytes)
{
    if (!m_request || id != m_request || m_revision != m_api.sessionRevision())
        return;
    m_request = 0;
    // Adapt only the Rust domain array. No upstream aliases or unit inference.
    const auto document = QJsonDocument::fromJson(bytes);
    if (!document.isArray())
        return;
    QList<Interval> intervals;
    for (const auto &value : document.array()) {
        const auto item = value.toObject();
        const auto start = item.value("start_ms").toInteger(-1);
        const auto end = item.value("end_ms").toInteger(-1);
        if (!value.isObject() || start < 0)
            continue;
        // Optional end bounds render as a point; normalization is presentation,
        // not a reason to reject the remaining domain records.
        intervals.append({start, qMax(start, end)});
    }
    m_intervals = std::move(intervals);
    rebuild();
}
void PlaybackClimaxModel::rebuild()
{
    QList<Mark> sorted, marks;
    if (m_duration > 0) {
        for (const auto &interval : m_intervals) {
            if (interval.start <= 0 || interval.start >= m_duration)
                continue;
            sorted.append(
                {double(interval.start) / m_duration,
                 double(std::clamp(interval.end, interval.start, m_duration)) / m_duration});
        }
        std::stable_sort(sorted.begin(), sorted.end(),
                         [](const Mark &a, const Mark &b) { return a.start < b.start; });
        // EchoMusic compares adjacent entries in the complete sorted list,
        // not the previously accepted entry; preserve that 0.2% spacing rule.
        for (int i = 0; i < sorted.size(); ++i)
            if (i == 0 || sorted[i].start - sorted[i - 1].start > .002)
                marks.append(sorted[i]);
    }
    if (marks == m_marks)
        return;
    beginResetModel();
    m_marks = std::move(marks);
    endResetModel();
    emit changed();
}
} // namespace EchoMusic
