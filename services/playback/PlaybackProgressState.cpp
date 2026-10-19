#include "PlaybackProgressState.h"
#include <algorithm>
#include <cmath>
namespace EchoMusic
{
void PlaybackProgressState::reset()
{
    *this = {};
}
void PlaybackProgressState::observe(const QJsonObject &event)
{
    const auto kind = event.value("event").toString();
    if (kind != "seek" && kind != "seeked" && kind != "time-update" &&
        kind != "core-state-change" && kind != "ao-state-change")
        return;
    // Context-free synthetic positions (including load/seek confirmations) do
    // not prove that the audio output sample counter has advanced.
    if (event.value("track_seq").toDouble() <= 0 || !event.value("event_id").isDouble() ||
        !event.value("generation").isDouble())
        return;
    const auto generation = quint64(event.value("generation").toDouble());
    const auto revision = quint64(event.value("event_id").toDouble());
    if (generation < m_generation)
        return;
    if (generation > m_generation) {
        // A fallback decoder can advance generation before acknowledging the
        // same seek. Retire snapshots, but keep seeking until its newer completion.
        const auto seeking = m_seek;
        reset();
        m_seek = seeking;
        m_generation = generation;
    }
    if (kind == "seek" && revision >= m_seekRevision) {
        m_seek = true;
        m_seekRevision = revision;
        m_position = event.value("time").toDouble();
    } else if (kind == "seeked" && revision >= m_seekRevision) {
        m_seek = false;
        m_seekRevision = revision;
        m_position = event.value("time").toDouble();
        m_progressRevision = std::max(m_progressRevision, revision);
    } else if (kind == "time-update") {
        const auto position = event.value("time").toDouble();
        if (position > m_position + .001) {
            m_position = position;
            // AudioSink coalesces time events in place: arrival order is NOT
            // native order. event_id prevents old AO/core snapshots reviving busy.
            m_progressRevision = std::max(m_progressRevision, revision);
        }
    } else if (kind == "core-state-change" && revision >= m_coreRevision) {
        m_core = event.value("core_state").toString();
        m_coreRevision = revision;
        if (m_core == "idle" || m_core == "stopped" || m_core == "error" || m_core == "device-lost")
            m_seek = false;
    } else if (kind == "ao-state-change" && revision >= m_aoRevision) {
        m_aoPaused = event.value("ao_paused").toBool();
        const auto buffering = event.value("ao_buffering_state");
        m_buffering = buffering.isDouble() ? buffering.toDouble() : 100;
        m_aoRevision = revision;
    }
}
PlaybackProgressState::Reason PlaybackProgressState::reason(bool available, bool playing) const
{
    if (!available)
        return Ready;
    if (m_seek ||
        (m_core == "seeking" && std::max(m_progressRevision, m_seekRevision) <= m_coreRevision))
        return Seeking;
    return playing && m_core == "buffering" && m_aoPaused && std::isfinite(m_buffering) &&
                   m_buffering < 100 && m_coreRevision && m_aoRevision &&
                   m_progressRevision <= std::max(m_coreRevision, m_aoRevision)
               ? Buffering
               : Ready;
}
} // namespace EchoMusic
