#include "PluginSurfaceModel.h"
namespace EchoMusic
{
QVariantList PluginSurfaceModel::entries() const
{
    QVariantList values;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
        if (it->scope)
            values.append(QVariantMap{{"key", it.key()},
                                      {"target", it->target},
                                      {"component", it->component},
                                      {"scope", QVariant::fromValue(it->scope.data())}});
    return values;
}
void PluginSurfaceModel::insert(const QString &key, const QString &target, const QUrl &component,
                                QObject *scope)
{
    if (!scope || m_entries.size() >= 64)
        return;
    m_entries.insert(key, {target, component, scope});
    emit changed();
}
void PluginSurfaceModel::remove(const QString &key)
{
    if (m_entries.remove(key))
        emit changed();
}
} // namespace EchoMusic
