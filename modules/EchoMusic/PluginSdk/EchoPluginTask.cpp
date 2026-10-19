#include "EchoPluginTask.h"
#include "EchoPluginContext.h"
#include "ShellController.h"
#include <cmath>
namespace EchoMusic
{
EchoPluginTask::EchoPluginTask(EchoPluginContext &scope, EchoPluginRegistration &lease,
                               QString title)
    : QObject(&lease), m_scope(scope), m_lease(lease)
{
    m_content.name = std::move(title);
    connect(&lease, &EchoPluginRegistration::activeChanged, this, &EchoPluginTask::activeChanged);
    lease.stage(
        [this] {
            TaskCenterModel::Registration task;
            task.id = m_lease.key();
            task.content = m_content;
            task.cancel = [this] {
                if (m_scope.active() && active())
                    emit cancelRequested();
            };
            m_handle = m_scope.host().shell.tasks()->registerTask(m_lease, std::move(task));
            return m_handle.generation != 0;
        },
        [this] { m_scope.host().shell.tasks()->remove(m_handle); });
    if (scope.committed())
        lease.commit();
}
bool EchoPluginTask::setProgress(double percent, const QString &label)
{
    if (!active() || !m_scope.active() || m_terminal || !std::isfinite(percent) || percent < 0 ||
        percent > 100)
        return false;
    m_content.percent = percent;
    m_content.statusLabel = label;
    return !m_handle.generation ||
           m_scope.host().shell.tasks()->updateProgress(m_handle, percent, label);
}
bool EchoPluginTask::finish(TaskCenterModel::Status status, const QString &message)
{
    if (!active() || !m_scope.active() || m_terminal || !m_handle.generation)
        return false;
    m_content.error = message;
    m_terminal = m_scope.host().shell.tasks()->finish(m_handle, status, m_content);
    return m_terminal;
}
bool EchoPluginTask::complete()
{
    return finish(TaskCenterModel::Completed, {});
}
bool EchoPluginTask::fail(const QString &message)
{
    return finish(TaskCenterModel::Error, message);
}
bool EchoPluginTask::abort()
{
    return finish(TaskCenterModel::Aborted, {});
}
void EchoPluginTask::dispose()
{
    m_lease.dispose();
}
} // namespace EchoMusic
