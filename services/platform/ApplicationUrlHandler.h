#pragma once
#include <QObject>
#include <QUrl>
namespace EchoMusic
{
/// Qt-native URL delivery. Queues startup FileOpen events until services exist;
/// typed share validation belongs to the incoming-share coordinator; plugin
/// source approval remains with PluginManager before any install/code.
class ApplicationUrlHandler final : public QObject
{
    Q_OBJECT
public:
    explicit ApplicationUrlHandler(QObject *parent = nullptr);
    QList<QUrl> takePending(qsizetype maximum = 16);
    void beginDelivery();
signals:
    void urlRequested(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QList<QUrl> m_pending;
    bool m_ready = false;
};
} // namespace EchoMusic
