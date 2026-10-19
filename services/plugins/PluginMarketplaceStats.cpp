#include "PluginMarketplaceStats.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
namespace EchoMusic
{
QVariantMap PluginMarketplaceStats::parse(const QByteArray &bytes, const QSet<QString> &requested,
                                          QString &error)
{
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    const auto root = document.object();
    if (parse.error != QJsonParseError::NoError || !document.isObject() ||
        root.value("ok") != QJsonValue(true) || !root.value("plugins").isArray()) {
        error = "Invalid plugin statistics response";
        return {};
    }
    QVariantMap results;
    for (const auto &value : root.value("plugins").toArray()) {
        const auto row = value.toObject();
        const auto stats = row.value("stats").toObject();
        const auto source = row.value("sourceId").toString();
        const auto id = row.value("pluginId").toString();
        const auto key = source + ':' + id;
        if (!requested.contains(key))
            continue;
        for (const auto *field : {"installCount", "updateCount", "failureCount", "score"}) {
            const auto number = stats.value(field);
            if (!number.isDouble() || !std::isfinite(number.toDouble()) || number.toDouble() < 0) {
                error = "Invalid plugin statistics number";
                return {};
            }
        }
        for (const auto *field : {"lastInstalledAt", "lastUpdatedAt"})
            if (!stats.value(field).isString()) {
                error = "Invalid plugin statistics timestamp";
                return {};
            }
        if (results.contains(key)) {
            error = "Duplicate plugin statistics identity";
            return {};
        }
        results.insert(key, stats.toVariantMap());
    }
    return results;
}
} // namespace EchoMusic
