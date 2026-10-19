#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Presentation of a measured instrumental interval. PlaybackService's media
/// timeline is the only clock: pause, speed and seek use the same projection as
/// the text. No free-running timer and no mutable lyric/model state live here.
class LyricInterlude : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal timeMs READ timeMs WRITE setTimeMs NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(qreal opacity READ opacity NOTIFY changed)
    Q_PROPERTY(qreal scaleFactor READ scaleFactor NOTIFY changed)
    Q_PROPERTY(qreal firstOpacity READ firstOpacity NOTIFY changed)
    Q_PROPERTY(qreal secondOpacity READ secondOpacity NOTIFY changed)
    Q_PROPERTY(qreal thirdOpacity READ thirdOpacity NOTIFY changed)
public:
    explicit LyricInterlude(QObject *parent = nullptr) : QObject(parent) {}
    Q_INVOKABLE void configure(qreal start, qreal end, qreal now, bool intro, bool restart);
    qreal timeMs() const
    {
        return m_time;
    }
    void setTimeMs(qreal value);
    bool available() const
    {
        return m_available;
    }
    qreal opacity() const
    {
        return m_opacity;
    }
    qreal scaleFactor() const
    {
        return m_scale;
    }
    qreal firstOpacity() const
    {
        return m_dots[0];
    }
    qreal secondOpacity() const
    {
        return m_dots[1];
    }
    qreal thirdOpacity() const
    {
        return m_dots[2];
    }
signals:
    void changed();

private:
    void evaluate();
    qreal m_start = -1, m_end = -1, m_anchor = 0, m_time = 0, m_hold = 500;
    qreal m_opacity = 0, m_scale = 1, m_dots[3] = {0, 0, 0};
    bool m_available = false;
};
} // namespace EchoMusic
