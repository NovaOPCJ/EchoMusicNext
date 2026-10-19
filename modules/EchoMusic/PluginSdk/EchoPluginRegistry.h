#pragma once
#include "EchoPluginOperation.h"
#include "EchoPluginRegistration.h"
#include <QJSValue>
#include <QHash>
#include <QPointer>
#include <functional>

namespace EchoMusic
{
class EchoPluginContext;
/// Scope-owned, versioned service calls between explicitly declared plugin
/// dependencies. No provider QObject, writable store or QJSValue escapes.
class EchoPluginRegistry final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by EchoPluginContext")
public:
    explicit EchoPluginRegistry(EchoPluginContext &scope);
    Q_INVOKABLE bool isAvailable(const QString &pluginId) const;
    Q_INVOKABLE QString version(const QString &pluginId) const;
    Q_INVOKABLE QString serviceVersion(const QString &pluginId, const QString &serviceId) const;
    Q_INVOKABLE EchoMusic::EchoPluginRegistration *
    registerService(const QString &id, const QString &version, const QJSValue &methods);
    Q_INVOKABLE EchoMusic::EchoPluginOperation *
    call(const QString &pluginId, const QString &serviceId, const QString &versionRange,
         const QString &method, const QVariantMap &arguments = {});
    void setResolver(std::function<EchoPluginContext *(const QString &)> resolver);
signals:
    void availabilityChanged();

private:
    struct Service
    {
        QString version;
        QHash<QString, QJSValue> methods;
        QPointer<EchoPluginRegistration> lease;
    };
    EchoPluginContext *provider(const QString &pluginId) const;
    EchoPluginContext &m_scope;
    QHash<QString, Service> m_services;
    std::function<EchoPluginContext *(const QString &)> m_resolver;
};
} // namespace EchoMusic
