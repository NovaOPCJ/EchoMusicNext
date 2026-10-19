#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <optional>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Presentation-only damped oscillator, sampled on Qt's animation frames.
/// Closed-form integration keeps the same trajectory at 60/120Hz and when a
/// frame is missed. Retargeting retains position and velocity, recycling snaps.
class LyricSpring : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(double value READ value NOTIFY valueChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
public:
    explicit LyricSpring(QObject *parent = nullptr) : QObject(parent)
    {
        m_clock.start();
    }
    double value() const
    {
        return m_value;
    }
    bool running() const
    {
        return m_running;
    }
    Q_INVOKABLE void snap(double value);
    /// Preserve screen position/velocity when the virtual content prefix moves.
    Q_INVOKABLE void shift(double delta);
    Q_INVOKABLE void retarget(double target, double intervalMs, int delayMs, bool spring);
    Q_INVOKABLE void retargetScale(double target, bool animate);
    Q_INVOKABLE void advance();
signals:
    void valueChanged();
    void runningChanged();

private:
    struct Pending
    {
        double target, intervalMs;
        qint64 due;
        bool spring;
    };
    void begin(double target, double intervalMs, bool spring, qint64 time);
    bool evaluate(qint64 time);
    std::optional<Pending> m_pending;
    QElapsedTimer m_clock;
    qint64 m_begin = 0;
    double m_value = 0, m_origin = 0, m_target = 0, m_velocity = 0, m_initialVelocity = 0,
           m_k = 140, m_c = 22;
    bool m_running = false, m_spring = true;
};
} // namespace EchoMusic
