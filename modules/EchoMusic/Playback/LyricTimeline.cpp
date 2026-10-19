#include "LyricTimeline.h"
#include <algorithm>
#include <cmath>
namespace EchoMusic
{
namespace
{
class PresentationRows final : public QSortFilterProxyModel
{
public:
    explicit PresentationRows(LyricTimeline *timeline)
        : QSortFilterProxyModel(timeline), m_timeline(timeline)
    {
        setDynamicSortFilter(false);
    }
    QHash<int, QByteArray> roleNames() const override
    {
        auto roles = QSortFilterProxyModel::roleNames();
        roles.insert(Qt::UserRole + 100, "sourceRow");
        return roles;
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        return role == Qt::UserRole + 100 ? QVariant(mapToSource(index).row())
                                          : QSortFilterProxyModel::data(index, role);
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &) const override
    {
        return m_timeline->ordinal(row) >= 0;
    }
    bool lessThan(const QModelIndex &a, const QModelIndex &b) const override
    {
        return m_timeline->ordinal(a.row()) < m_timeline->ordinal(b.row());
    }

private:
    LyricTimeline *m_timeline;
};
} // namespace
LyricTimeline::LyricTimeline(QObject *parent) : QObject(parent), m_rows(new PresentationRows(this))
{
}

void LyricTimeline::setModel(LyricModel *model)
{
    if (m_model == model)
        return;
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
    m_model = model;
    if (model) {
        connect(model, &LyricModel::changed, this, &LyricTimeline::rebuild);
        connect(model, &QObject::destroyed, this, &LyricTimeline::rebuild);
    }
    rebuild();
    emit modelChanged();
}
void LyricTimeline::setFilter(bool filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    rebuild();
    emit modelChanged();
}
void LyricTimeline::rebuild()
{
    m_bounds.clear();
    m_gaps.clear();
    m_orders.clear();
    m_dynamic = false;
    if (m_model) {
        m_orders.fill(-1, m_model->rowCount());
        for (int row = 0; row < m_model->rowCount(); ++row) {
            const auto line = m_model->lineAt(row);
            if (m_filter && line.value("filtered").toBool())
                continue;
            double start = line.value("startMs").toDouble();
            double end = start + line.value("durationMs").toDouble();
            const auto words = line.value("words").toList();
            if (!words.isEmpty()) {
                const auto first = words.first().toMap(), last = words.last().toMap();
                const bool untimedSingle = words.size() == 1 &&
                                           first.value("start_ms").toDouble() == 0 &&
                                           first.value("duration_ms").toDouble() == 0;
                if (!untimedSingle) {
                    start = first.value("start_ms").toDouble() + m_model->offsetMs();
                    end = last.value("start_ms").toDouble() + last.value("duration_ms").toDouble() +
                          m_model->offsetMs();
                }
            }
            m_dynamic |= words.size() > 1;
            m_bounds.append({row, start, end});
        }
    }
    std::stable_sort(m_bounds.begin(), m_bounds.end(),
                     [](const auto &a, const auto &b) { return a.start < b.start; });
    // A short timing collision is a transcription seam, not a duet. Only the
    // presentation interval is trimmed; every original word remains untouched.
    for (int i = 0; i < m_bounds.size(); ++i) {
        for (int j = i + 1; j < m_bounds.size() && m_bounds[j].start < m_bounds[i].end; ++j) {
            const double overlap = m_bounds[i].end - m_bounds[j].start;
            if (overlap < 500 &&
                (overlap <= 100 || overlap <= (m_bounds[j].end - m_bounds[j].start) * .1)) {
                m_bounds[i].end = m_bounds[j].start;
                break;
            }
        }
    }
    const double offset = m_model ? m_model->offsetMs() : 0;
    double groupStart = offset, groupEnd = offset, previousStart = offset, previousEnd = offset;
    for (int i = 0; i < m_bounds.size(); ++i) {
        auto &bound = m_bounds[i];
        const double rawStart = bound.start, rawEnd = bound.end;
        const double overlapDuration = previousEnd - rawStart;
        const bool overlap = i > 0 && overlapDuration > 0;
        const double lead = !overlap ? 600. : overlapDuration < 400 ? overlapDuration * .7 : 400.;
        const double boundary = i == 0 ? offset : overlap ? previousStart : groupEnd;
        bound.start = std::min(rawStart, std::max(boundary, rawStart - lead));
        if (i > 0 && rawStart < groupEnd && rawEnd > groupStart) {
            groupStart = std::min(groupStart, rawStart);
            groupEnd = std::max(groupEnd, rawEnd);
        } else {
            groupStart = rawStart;
            groupEnd = rawEnd;
        }
        previousStart = bound.start;
        previousEnd = rawEnd;
        m_orders[bound.row] = i;
    }
    double end = offset;
    for (int i = 0; i < m_bounds.size(); ++i) {
        if (m_bounds[i].start - end >= 7000)
            m_gaps.append({i - 1, end, m_bounds[i].start});
        end = std::max(end, m_bounds[i].end);
    }
    m_maxEnd = end;
    m_rows->setSourceModel(m_model);
    m_rows->invalidate();
    m_rows->sort(0);
    reset(m_time);
}
void LyricTimeline::setTimeMs(double time)
{
    if (!std::isfinite(time) || time == m_time)
        return;
    update(time, time < m_time);
    emit timeChanged();
}
void LyricTimeline::reset(double time)
{
    if (!std::isfinite(time))
        return;
    update(time, true);
    emit timeChanged();
}
void LyricTimeline::update(double time, bool seek)
{
    const auto oldHighlight = m_highlighted;
    const int oldFocus = m_focus, oldGap = m_gap, oldLatest = m_latest;
    if (seek) {
        m_cursor = 0;
        m_gapCursor = 0;
        m_playing.clear();
        m_highlighted.clear();
        m_focus = 0;
    }
    while (m_gapCursor < m_gaps.size() && time >= m_gaps[m_gapCursor].end)
        ++m_gapCursor;
    m_gap = m_gapCursor < m_gaps.size() && time >= m_gaps[m_gapCursor].start ? m_gapCursor : -1;
    for (auto it = m_playing.begin(); it != m_playing.end();) {
        if (m_bounds[*it].end <= time)
            it = m_playing.erase(it);
        else
            ++it;
    }
    bool added = false;
    while (m_cursor < m_bounds.size() && m_bounds[m_cursor].start <= time) {
        if (m_bounds[m_cursor].end > time) {
            m_playing.insert(m_cursor);
            added = true;
        }
        ++m_cursor;
    }
    if (seek) {
        // Reconstruct lingering overlap at the most recent nonempty line's
        // activation, so a seek and uninterrupted playback have the same state.
        int anchor = m_cursor - 1;
        while (anchor >= 0 && m_bounds[anchor].end <= m_bounds[anchor].start)
            --anchor;
        if (anchor >= 0) {
            for (int i = anchor; i >= 0; --i)
                if (m_bounds[i].end > m_bounds[anchor].start)
                    m_highlighted.insert(i);
            m_focus = m_highlighted.isEmpty()
                          ? anchor
                          : *std::min_element(m_highlighted.cbegin(), m_highlighted.cend());
        }
    } else if (added) {
        m_highlighted = m_playing;
        m_focus = *std::min_element(m_highlighted.cbegin(), m_highlighted.cend());
    }
    if ((m_gap >= 0 && m_playing.isEmpty()) || (!m_bounds.isEmpty() && time >= m_maxEnd))
        m_highlighted.clear();
    m_latest = m_highlighted.isEmpty()
                   ? m_focus
                   : *std::max_element(m_highlighted.cbegin(), m_highlighted.cend());
    m_time = time;
    if (seek || oldHighlight != m_highlighted || oldFocus != m_focus || oldGap != m_gap ||
        oldLatest != m_latest) {
        ++m_revision;
        emit changed();
    }
}
int LyricTimeline::rowFor(int order) const
{
    return order >= 0 && order < m_bounds.size() ? m_bounds[order].row : -1;
}
int LyricTimeline::focusIndex() const
{
    return rowFor(m_focus);
}
int LyricTimeline::latestIndex() const
{
    return rowFor(m_latest);
}
int LyricTimeline::gapAnchor() const
{
    return m_gap < 0 ? -2 : rowFor(m_gaps[m_gap].anchor);
}
double LyricTimeline::gapStart() const
{
    return m_gap < 0 ? -1 : m_gaps[m_gap].start;
}
double LyricTimeline::gapEnd() const
{
    return m_gap < 0 ? -1 : m_gaps[m_gap].end;
}
int LyricTimeline::ordinal(int row) const
{
    return row >= 0 && row < m_orders.size() ? m_orders[row] : -1;
}
bool LyricTimeline::highlighted(int row) const
{
    return m_highlighted.contains(ordinal(row));
}
bool LyricTimeline::active(int row) const
{
    const int i = ordinal(row);
    return i >= 0 && (m_highlighted.contains(i) || (i >= m_focus && i < m_latest));
}
double LyricTimeline::startAt(int row) const
{
    const int i = ordinal(row);
    return i < 0 ? -1 : m_bounds[i].start;
}
double LyricTimeline::endAt(int row) const
{
    const int i = ordinal(row);
    return i < 0 ? -1 : m_bounds[i].end;
}
} // namespace EchoMusic
