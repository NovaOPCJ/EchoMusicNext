#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Read-only presentation projection of the service timeline. No playback state
/// is written here. All elapsed time is monotonic; telemetry is not a frame tick.
class LyricClock : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit LyricClock(QObject *parent = nullptr) : QObject(parent)
    {
        m_clock.start();
    }
    Q_INVOKABLE void observe(double position, bool playing, double speed, bool reset = false);
    Q_INVOKABLE double sample() const;
    Q_INVOKABLE void setPlaybackState(bool playing, double speed);

private:
    double at(double now) const;
    QElapsedTimer m_clock;
    double m_observed = 0, m_correctionDuration = 250;
    double m_position = 0, m_correction = 0, m_speed = 1;
    bool m_playing = false, m_initialized = false;
};
} // namespace EchoMusic
