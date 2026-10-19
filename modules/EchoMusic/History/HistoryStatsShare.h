#pragma once
#include <QImage>
#include <QQuickItem>
#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQuickItem;
class QQuickItemGrabResult;
class QQuickWindow;
namespace EchoMusic
{
class HistoryService;
class PlaybackService;
/// Transient Qt Quick bitmap capture. No hidden export tree or image is retained
/// between actions; history and playback stay owned by their existing services.
class HistoryStatsShare final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by HistoryService")
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    HistoryStatsShare(HistoryService &, PlaybackService &, QObject *parent = nullptr);
    ~HistoryStatsShare() override;
    bool busy() const
    {
        return !m_stage.isNull();
    }
    Q_INVOKABLE void share(QQuickItem *page);
    Q_INVOKABLE void cancel();
signals:
    void busyChanged();
    void notice(const QString &message, bool warning);
    void captured(const QImage &image);
private slots:
    void tryCapture();

private:
    friend class HistoryStatsShareTestAccess;
    HistoryService &m_history;
    PlaybackService &m_playback;
    QPointer<QQuickItem> m_stage;
    QPointer<QQuickWindow> m_window;
    QSharedPointer<QQuickItemGrabResult> m_grab;
    QTimer m_imageDeadline;
    quint64 m_generation = 0;
    qreal m_uiScale = 1;
    bool m_prepared = false, m_imageExpired = false;
    QList<QMetaObject::Connection> m_connections;
};
} // namespace EchoMusic
