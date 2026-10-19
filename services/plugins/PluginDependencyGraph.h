#pragma once
#include "PluginPackage.h"
#include <QSet>

namespace EchoMusic
{
/// Immutable graph resolution on descriptors only; never executes module code.
/// One installed version owns each ID. Optional dependencies constrain versions
/// only when enabled. Ordering-only edges do not grant service access.
struct PluginDependencyGraph
{
    QStringList order;
    QMap<QString, QString> errors;
    static PluginDependencyGraph resolve(const QList<PluginPackage> &packages,
                                         const QSet<QString> &enabled);
};
} // namespace EchoMusic
