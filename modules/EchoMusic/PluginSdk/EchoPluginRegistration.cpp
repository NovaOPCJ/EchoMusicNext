#include "EchoPluginRegistration.h"
#include <utility>
namespace EchoMusic
{
EchoPluginRegistration::EchoPluginRegistration(QString key, quint64 revision, QObject *parent)
    : QObject(parent), m_key(std::move(key)), m_revision(revision)
{
}
void EchoPluginRegistration::stage(std::function<bool()> publish, std::function<void()> remove)
{
    m_publish = std::move(publish);
    m_remove = std::move(remove);
}
bool EchoPluginRegistration::commit()
{
    if (!m_active)
        return true;
    if (m_published)
        return true;
    m_published = !m_publish || m_publish();
    return m_published;
}
void EchoPluginRegistration::dispose()
{
    if (!std::exchange(m_active, false))
        return;
    if (m_published && m_remove)
        m_remove();
    m_publish = {};
    m_remove = {};
    emit activeChanged();
}
} // namespace EchoMusic
