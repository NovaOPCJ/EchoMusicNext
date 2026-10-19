#include "LyricClock.h"
#include <algorithm>
#include <cmath>
namespace EchoMusic
{
double LyricClock::at(double now) const
{
    const auto elapsed = std::max(0., now - m_observed);
    // Missing telemetry for a full second denotes a stalled source, rather than
    // permission to run indefinitely. Normal engine observations arrive at 250ms.
    return m_position + (m_playing ? std::min(1000., elapsed) * m_speed : 0) +
           m_correction * std::max(0., 1. - double(elapsed) / m_correctionDuration);
}
double LyricClock::sample() const
{
    return at((m_clock.nsecsElapsed() / 1000000.));
}
void LyricClock::setPlaybackState(bool playing, double speed)
{
    if (!std::isfinite(speed) || speed <= 0)
        return;
    const double now = m_clock.nsecsElapsed() / 1000000.;
    // Speed/pause changes are not new position measurements. Reanchoring at
    // the last telemetry sample would rewind up to an entire update interval.
    m_position = at(now);
    m_observed = now;
    m_correction = 0;
    m_playing = playing;
    m_speed = speed;
}
void LyricClock::observe(double position, bool playing, double speed, bool reset)
{
    if (!std::isfinite(position) || !std::isfinite(speed) || speed <= 0)
        return;
    const auto now = (m_clock.nsecsElapsed() / 1000000.);
    const double previous = at(now);
    // Only typed discontinuities reset the visible clock. A speed/DSP change
    // can delay an otherwise continuous observation by more than 200ms. Absorb
    // that error at a positive rate instead of treating it as a backwards seek.
    m_correction = !reset && m_initialized && playing && m_playing ? previous - position : 0;
    m_correctionDuration = std::max(250., 2 * std::max(0., m_correction) / speed);
    m_position = position;
    m_playing = playing;
    m_speed = speed;
    m_observed = now;
    m_initialized = true;
}
} // namespace EchoMusic
