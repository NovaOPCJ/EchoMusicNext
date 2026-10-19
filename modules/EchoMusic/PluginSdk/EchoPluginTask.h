#pragma once
#include "EchoPluginRegistration.h"
#include "TaskCenterModel.h"
namespace EchoMusic
{
class EchoPluginContext;
class EchoPluginTask final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by EchoPluginUi")
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
public:
    EchoPluginTask(EchoPluginContext &scope, EchoPluginRegistration &lease, QString title);
    bool active() const
    {
        return m_lease.active();
    }
    Q_INVOKABLE bool setProgress(double percent, const QString &label = {});
    Q_INVOKABLE bool complete();
    Q_INVOKABLE bool fail(const QString &message);
    Q_INVOKABLE bool abort();
    Q_INVOKABLE void dispose();
signals:
    void activeChanged();
    void cancelRequested();

private:
    bool finish(TaskCenterModel::Status status, const QString &message);
    EchoPluginContext &m_scope;
    EchoPluginRegistration &m_lease;
    TaskCenterModel::Handle m_handle;
    TaskCenterModel::Content m_content;
    bool m_terminal = false;
};
} // namespace EchoMusic
