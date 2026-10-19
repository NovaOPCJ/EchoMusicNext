#include "LyricSpring.h"
#include <algorithm>
#include <cmath>
namespace EchoMusic
{
void LyricSpring::shift(double delta)
{
    m_value += delta;
    m_origin += delta;
    m_target += delta;
    if (m_pending)
        m_pending->target += delta;
    emit valueChanged();
}
void LyricSpring::snap(double value)
{
    m_pending.reset();
    m_value = m_origin = m_target = value;
    m_velocity = m_initialVelocity = 0;
    if (m_running) {
        m_running = false;
        emit runningChanged();
    }
    emit valueChanged();
}
void LyricSpring::begin(double target, double intervalMs, bool spring, qint64 time)
{
    m_origin = m_value;
    m_initialVelocity = m_velocity;
    m_target = target;
    m_begin = time;
    m_spring = spring;
    const double ratio = std::pow((800 - std::clamp(intervalMs, 100., 800.)) / 700., .2);
    m_k = 170 + 50 * ratio;
    m_c = 2.2 * std::sqrt(m_k);
    if (intervalMs <= 0) {
        m_k = 90;
        m_c = 15;
    }
    // The reference's group-position spring retains mass 0.9 when policy
    // updates stiffness/damping. Store acceleration coefficients here.
    m_k /= .9;
    m_c /= .9;
}
void LyricSpring::retarget(double target, double intervalMs, int delayMs, bool spring)
{
    if (delayMs > 0 && m_pending && std::abs(target - m_pending->target) < .001 &&
        intervalMs == m_pending->intervalMs && spring == m_pending->spring)
        return;
    advance();
    const auto now = m_clock.elapsed();
    if (delayMs > 0) {
        // Stagger the target change, not the existing motion. Freezing and
        // restarting the old solver here makes every fast lyric lose velocity.
        begin(m_target, intervalMs, spring, now);
        m_pending = Pending{target, intervalMs, now + delayMs, spring};
    } else {
        m_pending.reset();
        if (std::abs(target - m_target) < .001) {
            advance();
            return;
        }
        begin(target, intervalMs, spring, now);
    }
    if (!m_running) {
        m_running = true;
        emit runningChanged();
    }
}
void LyricSpring::retargetScale(double target, bool animate)
{
    if (!animate) {
        snap(target);
        return;
    }
    advance();
    if (std::abs(target - m_target) < .001)
        return;
    m_pending.reset();
    begin(target, 0, true, m_clock.elapsed());
    m_k = 100. / 2;
    m_c = 25. / 2;
    if (!m_running) {
        m_running = true;
        emit runningChanged();
    }
}
bool LyricSpring::evaluate(qint64 time)
{
    const double t = std::max<qint64>(0, time - m_begin) / 1000.;
    const double displacement = m_origin - m_target;
    if (!m_spring) {
        const double p = std::min(1., t / .5);
        m_value = m_target + displacement * std::pow(1 - p, 3);
        m_velocity = 0;
    } else {
        double x, v;
        if (m_c * m_c >= 4 * m_k) {
            // Match the reference's critical response above critical damping.
            // A truly overdamped solver adds lag with the same tuning values.
            const double d = std::sqrt(m_k), b = m_initialVelocity + d * displacement;
            const double decay = std::exp(-d * t);
            x = (displacement + b * t) * decay;
            v = (b - d * (displacement + b * t)) * decay;
        } else {
            const double d = m_c / 2, w = std::sqrt(m_k - d * d);
            const double b = (m_initialVelocity + d * displacement) / w;
            x = std::exp(-d * t) * (displacement * std::cos(w * t) + b * std::sin(w * t));
            v = -d * x +
                std::exp(-d * t) * w * (-displacement * std::sin(w * t) + b * std::cos(w * t));
        }
        m_value = m_target + x;
        m_velocity = v;
    }
    if (std::abs(m_value - m_target) < .05 && std::abs(m_velocity) < .5) {
        m_value = m_target;
        m_velocity = 0;
        return false;
    }
    return true;
}
void LyricSpring::advance()
{
    if (!m_running)
        return;
    const auto now = m_clock.elapsed();
    if (m_pending && now >= m_pending->due) {
        // Evaluate at the scheduled switch time, then integrate the new target
        // to this frame. A missed frame must not restart the delay on delivery.
        const auto pending = *m_pending;
        evaluate(pending.due);
        m_pending.reset();
        begin(pending.target, pending.intervalMs, pending.spring, pending.due);
    }
    if (!evaluate(now) && !m_pending) {
        m_running = false;
        emit runningChanged();
    }
    emit valueChanged();
}
} // namespace EchoMusic
