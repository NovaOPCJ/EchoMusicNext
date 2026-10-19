#pragma once
#include <QJsonObject>

namespace EchoMusic
{
/// Read-only authoring contract from the actual Qt metaobjects and package
/// compatibility checks. Exporting it creates no services, scopes or databases.
QJsonObject pluginSdkContract();
} // namespace EchoMusic
