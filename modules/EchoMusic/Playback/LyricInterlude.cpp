#include "LyricInterlude.h"
#include <QEasingCurve>
#include <QPointF>
#include <iterator>
#include <algorithm>
#include <cmath>
namespace EchoMusic
{
namespace
{
QEasingCurve cubic(QPointF a, QPointF b)
{
    QEasingCurve curve(QEasingCurve::BezierSpline);
    curve.addCubicBezierSegment(a, b, {1, 1});
    return curve;
}
// Native cubic fit to measured breathing progression (development reference
// and error bounds in amll-interlude-observations.json). This uses Qt's spline
// evaluator, independently of the reference's clock, solver and renderer.
const QEasingCurve &breath()
{
    static const auto curve = [] {
        QEasingCurve c(QEasingCurve::BezierSpline);
        c.addCubicBezierSegment({.041666667, .000117847}, {.083333333, .000944377}, {.125, .0444});
        c.addCubicBezierSegment({.166666667, .087855623}, {.208333333, .178028547}, {.25, .266});
        c.addCubicBezierSegment({.291666667, .353971453}, {.333333333, .427308506}, {.375, .4624});
        c.addCubicBezierSegment({.416666667, .497491494}, {.458333333, .499851904}, {.5, .5});
        c.addCubicBezierSegment({.541666667, .500148096}, {.583333333, .500944377}, {.625, .5444});
        c.addCubicBezierSegment({.666666667, .587855623}, {.708333333, .678028547}, {.75, .766});
        c.addCubicBezierSegment({.791666667, .853971453}, {.833333333, .927308506}, {.875, .9624});
        c.addCubicBezierSegment({.916666667, .997491494}, {.958333333, .999821655}, {1, 1});
        return c;
    }();
    return curve;
}
qreal unit(qreal value)
{
    return std::clamp(value, 0., 1.);
}
} // namespace
void LyricInterlude::configure(qreal start, qreal end, qreal now, bool intro, bool restart)
{
    if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(now))
        return;
    if (start == m_start && end == m_end && !restart)
        return;
    m_start = start;
    m_end = end;
    m_anchor = restart ? now : start;
    m_time = now;
    m_hold = intro && !restart ? 0 : 500;
    m_available = start >= 0 && end > start && end - m_anchor - m_hold - 1000 >= 910;
    evaluate();
    emit changed();
}
void LyricInterlude::setTimeMs(qreal value)
{
    if (!std::isfinite(value) || m_time == value)
        return;
    m_time = value;
    evaluate();
    emit changed();
}
void LyricInterlude::evaluate()
{
    m_opacity = 0;
    m_scale = 1;
    std::fill(std::begin(m_dots), std::end(m_dots), 0.);
    if (!m_available || m_time < m_anchor + m_hold || m_time >= m_end)
        return;
    static const auto enter = cubic({.59, .02}, {.07, 1});
    static const auto lighting = cubic({.56, .01}, {.45, 1});
    static const auto expand = cubic({.14, .06}, {.25, 1});
    static const auto shrink = cubic({.29, .03}, {1, .38});
    static const auto fade = cubic({.43, .08}, {.83, .31});
    const qreal body = m_end - m_anchor - m_hold - 1000;
    const qreal t = m_time - m_anchor - m_hold;
    m_opacity = enter.valueForProgress(unit(t / 180));
    const bool shortBody = body < 3000;
    const qreal segment = std::round((body + 750) / 3), lastDuration = body - 2 * segment;
    const qreal lastTarget = shortBody ? 1 : lastDuration / segment;
    for (int i = 0; i < 3; ++i) {
        qreal fraction = 1;
        if (!shortBody)
            fraction = lighting.valueForProgress(
                           unit((t - i * segment) / (i == 2 ? lastDuration : segment))) *
                       (i == 2 ? lastTarget : 1);
        if (t >= body)
            fraction = i == 2 ? lastTarget + (1 - lastTarget) * unit((t - body) / 750) : 1;
        const qreal entrance = unit((t - i * 80) / 750);
        m_dots[i] = (.2 + .7 * unit(fraction)) * entrance * entrance;
    }
    if (t >= body) {
        const qreal tail = t - body;
        m_scale = tail < 750 ? 1 + .25 * expand.valueForProgress(unit(tail / 750))
                             : 1.25 - .85 * shrink.valueForProgress(unit((tail - 750) / 250));
        m_opacity *= 1 - fade.valueForProgress(unit((tail - 750) / 250));
    } else if (!shortBody) {
        const qreal period = body / std::max(1., std::floor(body / 4000));
        const qreal phase = breath().valueForProgress(std::fmod(t, period) / period);
        m_scale = 1 + .5 * std::min(phase, 1 - phase);
    }
}
} // namespace EchoMusic
