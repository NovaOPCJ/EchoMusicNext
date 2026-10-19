#include "PluginManager.h"
#include "SharingCommands.h"
#include "NetworkPreferences.h"
#include "PluginPackage.h"
#include "PluginMarketplaceStats.h"
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QDateTime>
#include <QCryptographicHash>
namespace EchoMusic
{
namespace
{
const QString statisticsEndpoint = "https://echomusic-plugin-marketplace.hoowhoami.dpdns.org";
const QString official = "https://github.com/hoowhoami/EchoMusicNextPlugins";
QString localPluginId(const QString &directory)
{
    QFile file(directory + "/manifest.json");
    if (!file.open(QIODevice::ReadOnly) || file.size() > 256 * 1024)
        return {};
    return PluginPackage::inspectManifest(file.readAll()).id;
}
QString raw(const QVariantMap &source, const QString &path)
{
    return "https://raw.githubusercontent.com/" + source.value("repository").toString() + "/HEAD/" +
           path;
}
} // namespace
PluginManager::PluginManager(PluginRuntime &runtime, QSettings &preferences, QObject *parent)
    : QObject(parent), m_runtime(runtime), m_preferences(preferences), m_catalog(this),
      m_sourceModel(this), m_network(this)
{
    m_sources = preferences.value("plugins/sources").toList();
    bool found = false;
    for (const auto &value : m_sources)
        if (value.toMap().value("url") == official)
            found = true;
    if (!found)
        m_sources.prepend(normalizeSource(official, tr("EchoMusicNext 官方插件源")));
    QFile cache(runtime.directory() + "/catalog.json");
    if (cache.open(QIODevice::ReadOnly) && cache.size() <= 8 * 1024 * 1024) {
        const auto doc = QJsonDocument::fromJson(cache.readAll()).object();
        if (doc.value("schemaVersion").toInt() == 2) {
            m_entries = doc.value("plugins").toArray().toVariantList();
            m_fetchedAt = doc.value("fetchedAt").toString();
            m_stats = doc.value("statistics").toObject().toVariantMap();
            m_statsAt = qint64(doc.value("statisticsAt").toDouble());
            m_statsRetryAt = qint64(doc.value("statisticsRetryAt").toDouble());
        }
    }
    connect(&runtime, &PluginRuntime::catalogChanged, this, &PluginManager::rebuild);
    connect(&runtime, &PluginRuntime::managementStarted, this,
            [this](const QString &installation, const QString &pluginId) {
                if (m_busy && pluginId == m_pendingPluginId)
                    m_pendingInstallation = installation;
            });
    connect(&runtime, &PluginRuntime::managementFinished, this,
            [this](const QString &installation, bool ok, const QString &error) {
                if (m_busy && installation == m_pendingInstallation)
                    finishInstall(ok, error);
                else
                    emit notice(ok ? (m_runtime.record(installation).isEmpty() ? tr("插件已卸载")
                                                                               : tr("插件已安装"))
                                   : error,
                                !ok);
            });
    connect(this, &PluginManager::changed, this, &PluginManager::resolveShared);
    rebuild();
}
PluginManager::~PluginManager()
{
    shutdown();
}
bool PluginManager::safePath(const QString &path)
{
    if (path.isEmpty() || path.size() > 1024 || path.contains('\\') || path.contains(':') ||
        path.startsWith('/') || path.endsWith('/'))
        return false;
    for (const auto &part : path.split('/'))
        if (part.isEmpty() || part == "." || part == ".." || part.startsWith('.'))
            return false;
    return true;
}
QVariantMap PluginManager::normalizeSource(const QString &text, const QString &name)
{
    QUrl url(text.trimmed());
    if (url.scheme() != "https" || url.host().toLower() != "github.com" ||
        !url.userInfo().isEmpty() || url.port() != -1 || url.hasQuery() || url.hasFragment())
        return {};
    const auto parts = url.path().split('/', Qt::SkipEmptyParts);
    if (parts.size() != 2 || !QRegularExpression("^[A-Za-z0-9_.-]+$").match(parts[0]).hasMatch() ||
        !QRegularExpression("^[A-Za-z0-9_.-]+$").match(parts[1]).hasMatch())
        return {};
    auto repo = parts[1];
    if (repo.endsWith(".git"))
        repo.chop(4);
    const auto repository = parts[0] + '/' + repo;
    const auto canonical = "https://github.com/" + repository;
    return {{"id", "github:" + repository.toLower()},
            {"name", name.trimmed().isEmpty() ? repo : name.trimmed().left(128)},
            {"url", canonical},
            {"repository", repository},
            {"enabled", true},
            {"official", canonical.compare(official, Qt::CaseInsensitive) == 0},
            {"pluginCount", 0},
            {"lastError", ""}};
}
void PluginManager::setNetworkPreferences(NetworkPreferences *network)
{
    m_networkPreferences = network;
}
void PluginManager::get(const QUrl &url, qint64 limit, Completion complete, const QByteArray &body,
                        int timeout)
{
    const auto target = body.isEmpty() && m_networkPreferences
                            ? m_networkPreferences->acceleratedGithubUrl(url)
                            : url;
    getAttempt(target, url, limit, std::move(complete), body, timeout, target != url);
}
void PluginManager::getAttempt(const QUrl &url, const QUrl &origin, qint64 limit,
                               Completion complete, const QByteArray &body, int timeout,
                               bool fallback)
{
    if (m_closing)
        return;
    QNetworkRequest request(m_resolveUrl ? m_resolveUrl(url) : url);
    request.setTransferTimeout(timeout);
    request.setRawHeader("User-Agent", "EchoMusicNext-Plugin-Marketplace/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    if (!body.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto *reply = body.isEmpty() ? m_network.get(request) : m_network.post(request, body);
    m_replies.insert(reply);
    auto bytes = std::make_shared<QByteArray>();
    auto tooLarge = std::make_shared<bool>(false);
    connect(reply, &QIODevice::readyRead, this, [reply, bytes, limit, tooLarge] {
        bytes->append(reply->read(limit - bytes->size() + 1));
        if (bytes->size() > limit) {
            *tooLarge = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, bytes, limit, tooLarge, complete = std::move(complete), origin, body,
             timeout, fallback] {
                m_replies.remove(reply);
                if (reply->isOpen())
                    bytes->append(reply->read(limit - bytes->size() + 1));
                const auto status =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                QString error = *tooLarge || bytes->size() > limit ? tr("响应超过大小限制")
                                : reply->error() != QNetworkReply::NoError || status != 200
                                    ? tr("HTTP %1：%2").arg(status).arg(reply->errorString())
                                    : QString();
                reply->deleteLater();
                if (!m_closing && fallback && !error.isEmpty())
                    getAttempt(origin, origin, limit, std::move(complete), body, timeout, false);
                else if (!m_closing)
                    complete(*bytes, error);
            });
}
void PluginManager::fetchSource(QVariantMap source, quint64 generation,
                                std::function<void(QVariantMap, QVariantList)> complete)
{
    get(QUrl(raw(source, "echo-plugins.json")), 256 * 1024,
        [this, source, generation, complete](QByteArray bytes, QString error) mutable {
            if (generation != m_generation)
                return;
            QJsonParseError parse;
            const auto doc = QJsonDocument::fromJson(bytes, &parse).object();
            if (error.isEmpty() &&
                (parse.error != QJsonParseError::NoError ||
                 doc.value("schemaVersion").toInt() != 2 || !doc.value("plugins").isArray() ||
                 doc.value("plugins").toArray().size() > 512))
                error = tr("插件索引无效");
            if (!error.isEmpty()) {
                source["lastError"] = error;
                complete(source, {});
                return;
            }
            auto paths = std::make_shared<QStringList>();
            auto artifacts = std::make_shared<QHash<QString, QVariantMap>>();
            for (const auto &value : doc.value("plugins").toArray()) {
                const auto path = value.toObject().value("path").toString();
                const auto artifact = value.toObject().value("artifact").toString();
                const auto checksum = value.toObject().value("sha256").toString();
                if (!safePath(path) || paths->contains(path) || !safePath(artifact) ||
                    !artifact.endsWith(".echomod", Qt::CaseInsensitive) ||
                    !QRegularExpression("^[a-f0-9]{64}$").match(checksum).hasMatch()) {
                    source["lastError"] = tr("插件索引路径无效或重复");
                    complete(source, {});
                    return;
                }
                paths->append(path);
                artifacts->insert(path, {{"artifact", artifact}, {"sha256", checksum}});
            }
            auto results = std::make_shared<QVariantList>();
            auto current = std::make_shared<int>(0);
            auto next = std::make_shared<std::function<void()>>();
            // Serial per-source requests bound memory and preserve index order.
            // Break the callback cycle on every terminal path.
            std::weak_ptr<std::function<void()>> weakNext = next;
            *next = [this, source, generation, complete, paths, artifacts, results, current,
                     weakNext]() mutable {
                const auto next = weakNext.lock();
                if (!next)
                    return;
                if (generation != m_generation || m_closing) {
                    *next = {};
                    return;
                }
                if (*current >= paths->size()) {
                    source["lastError"] = "";
                    source["pluginCount"] = results->size();
                    source["lastFetchedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
                    auto finish = complete;
                    *next = {};
                    finish(source, *results);
                    return;
                }
                const auto path = paths->at((*current)++);
                get(QUrl(raw(source, path + "/manifest.json")), 256 * 1024,
                    [this, source, generation, complete, path, artifacts, results,
                     next](QByteArray bytes, QString error) mutable {
                        if (generation != m_generation) {
                            *next = {};
                            return;
                        }
                        auto package = PluginPackage::inspectManifest(bytes);
                        if (error.isEmpty() && !package.error.isEmpty() && !package.incompatible)
                            error = package.error;
                        if (!error.isEmpty()) {
                            source["lastError"] = error;
                            auto finish = complete;
                            *next = {};
                            finish(source, {});
                            return;
                        }
                        auto record = package.manifest;
                        record.insert("key", source.value("id").toString() + ':' + package.id);
                        record.insert("sourceId", source.value("id"));
                        record.insert("sourceName", source.value("name"));
                        record.insert("sourceUrl", source.value("url"));
                        record.insert("repository", source.value("repository"));
                        record.insert("path", path);
                        record.insert("artifact", artifacts->value(path).value("artifact"));
                        record.insert("sha256", artifacts->value(path).value("sha256"));
                        record.insert("compatible", package.error.isEmpty());
                        record.insert("error", package.error);
                        record.insert(
                            "iconUrl",
                            package.manifest.value("icon").toString().isEmpty()
                                ? QString()
                                : raw(source,
                                      path + '/' + package.manifest.value("icon").toString()));
                        record.insert("manifest", package.manifest);
                        for (const auto &old : *results)
                            if (old.toMap().value("id") == record.value("id")) {
                                source["lastError"] = tr("插件索引包含重复ID");
                                auto finish = complete;
                                *next = {};
                                finish(source, {});
                                return;
                            }
                        results->append(record);
                        auto continueFetch = *next;
                        continueFetch();
                    });
            };
            auto begin = *next;
            begin();
        });
}
void PluginManager::refreshCatalog(bool force)
{
    if (force)
        m_shareInstallError.clear();
    if (m_closing || m_catalogLoading || m_busy || (m_loaded && !force))
        return;
    m_catalogLoading = true;
    emit changed();
    const auto generation = ++m_generation;
    auto pending = std::make_shared<int>(0);
    for (const auto &value : m_sources)
        if (value.toMap().value("enabled").toBool())
            ++*pending;
    if (!*pending) {
        m_catalogLoading = false;
        m_loaded = true;
        rebuild();
        save();
        return;
    }
    for (const auto &value : m_sources) {
        auto source = value.toMap();
        if (!source.value("enabled").toBool())
            continue;
        fetchSource(source, generation,
                    [this, generation, pending, force](QVariantMap source, QVariantList plugins) {
                        if (generation != m_generation)
                            return;
                        const auto id = source.value("id");
                        for (auto &value : m_sources)
                            if (value.toMap().value("id") == id)
                                value = source;
                        if (source.value("lastError").toString().isEmpty()) {
                            for (int i = int(m_entries.size()) - 1; i >= 0; --i)
                                if (m_entries[i].toMap().value("sourceId") == id)
                                    m_entries.removeAt(i);
                            m_entries.append(plugins);
                        }
                        if (--*pending == 0) {
                            m_catalogLoading = false;
                            m_loaded = true;
                            m_fetchedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
                            save();
                            refreshStatistics(force);
                        }
                        rebuild();
                    });
    }
}
QVariantList PluginManager::sourceOptions() const
{
    QVariantList values{QVariantMap{{"label", tr("全部源")}, {"value", ""}}};
    for (const auto &value : m_sources) {
        const auto source = value.toMap();
        values.append(QVariantMap{{"label", source.value("name")},
                                  {"value", source.value("id")},
                                  {"disabled", !source.value("enabled").toBool()}});
    }
    return values;
}
QString PluginManager::sourceSummary() const
{
    const auto enabled = std::count_if(m_sources.cbegin(), m_sources.cend(), [](const auto &value) {
        return value.toMap().value("enabled").toBool();
    });
    return m_sources.isEmpty() ? tr("暂无插件源")
                               : tr("%1/%2 个源启用").arg(enabled).arg(m_sources.size());
}
QStringList PluginManager::sourceNames() const
{
    QStringList values{tr("全部插件源")};
    for (const auto &source : m_sources)
        if (source.toMap().value("enabled").toBool())
            values.append(source.toMap().value("name").toString());
    return values;
}
QStringList PluginManager::sourceIds() const
{
    QStringList values{""};
    for (const auto &source : m_sources)
        if (source.toMap().value("enabled").toBool())
            values.append(source.toMap().value("id").toString());
    return values;
}
QString PluginManager::sourceError() const
{
    for (const auto &value : m_sources) {
        auto s = value.toMap();
        if (s.value("enabled").toBool() && !s.value("lastError").toString().isEmpty())
            return s.value("name").toString() + "：" + s.value("lastError").toString();
    }
    return {};
}
int PluginManager::catalogCount() const
{
    int count = 0;
    const auto enabled = sourceIds();
    for (const auto &entry : m_entries)
        if (enabled.contains(entry.toMap().value("sourceId").toString()))
            ++count;
    return count;
}
int PluginManager::updateCount() const
{
    int count = 0;
    QSet<QString> ids;
    for (const auto &value : m_entries) {
        const auto e = value.toMap();
        const auto current =
            m_runtime.record(m_runtime.installationForId(e.value("id").toString()));
        if (!current.isEmpty() && e.value("compatible").toBool() &&
            QVersionNumber::fromString(e.value("version").toString()) >
                QVersionNumber::fromString(current.value("version").toString()) &&
            !ids.contains(e.value("id").toString()) &&
            sourceIds().contains(e.value("sourceId").toString())) {
            ++count;
            ids.insert(e.value("id").toString());
        }
    }
    return count;
}
void PluginManager::rebuild()
{
    QVariantList rows;
    const auto enabled = sourceIds();
    for (const auto &value : m_entries) {
        auto e = value.toMap();
        if (!enabled.contains(e.value("sourceId").toString()) ||
            (!m_sourceFilter.isEmpty() && e.value("sourceId") != m_sourceFilter))
            continue;
        const auto text = e.value("name").toString() + ' ' + e.value("id").toString() + ' ' +
                          e.value("author").toString() + ' ' + e.value("sourceName").toString() +
                          ' ' + e.value("description").toString() + ' ' +
                          e.value("tags").toStringList().join(' ');
        if (!text.contains(m_search.trimmed(), Qt::CaseInsensitive))
            continue;
        const auto current =
            m_runtime.record(m_runtime.installationForId(e.value("id").toString()));
        const bool installed = !current.isEmpty();
        const bool update =
            installed && QVersionNumber::fromString(e.value("version").toString()) >
                             QVersionNumber::fromString(current.value("version").toString());
        const auto origin = current.value("origin").toMap();
        if (installed &&
            (origin.value("kind") != "marketplace" || origin.value("id") != e.value("sourceId"))) {
            e["compatible"] = false;
            e["error"] = tr("相同插件 ID 已由其他来源安装，请先卸载；不同来源不会共享私有数据");
        }
        e["installed"] = installed;
        e["updateAvailable"] = update;
        e["installedVersion"] = current.value("version");
        e["installLabel"] = !e.value("compatible").toBool() ? tr("不兼容")
                            : installed                     ? update ? tr("更新") : tr("已安装")
                                                            : tr("安装");
        e["status"] = !e.value("compatible").toBool() ? tr("不兼容")
                      : update                        ? tr("可更新")
                                                      : QString();
        e["busy"] = m_busy && e.value("key") == m_installingKey;
        e["stats"] = m_stats.value(e.value("key").toString());
        rows.append(e);
    }
    m_catalog.replace(rows);
    m_sourceModel.replace(m_sources);
    emit changed();
}
void PluginManager::save()
{
    m_preferences.setValue("plugins/sources", m_sources);
    m_preferences.sync();
    QDir().mkpath(m_runtime.directory());
    QSaveFile cache(m_runtime.directory() + "/catalog.json");
    if (cache.open(QIODevice::WriteOnly)) {
        cache.write(QJsonDocument(QJsonObject{{"schemaVersion", 2},
                                              {"plugins", QJsonArray::fromVariantList(m_entries)},
                                              {"fetchedAt", m_fetchedAt},
                                              {"statistics", QJsonObject::fromVariantMap(m_stats)},
                                              {"statisticsAt", double(m_statsAt)},
                                              {"statisticsRetryAt", double(m_statsRetryAt)}})
                        .toJson());
        cache.commit();
    }
}
void PluginManager::setSearch(const QString &value)
{
    if (m_search == value)
        return;
    m_search = value;
    rebuild();
}
void PluginManager::setSourceFilter(const QString &value)
{
    if (m_sourceFilter == value)
        return;
    m_sourceFilter = value;
    rebuild();
}
bool PluginManager::addSource(const QString &url, const QString &name)
{
    if (m_catalogLoading || m_busy || m_closing)
        return false;
    const auto source = normalizeSource(url, name);
    if (source.isEmpty()) {
        emit notice(tr("请输入有效的GitHub仓库地址"), true);
        return false;
    }
    for (const auto &value : m_sources)
        if (value.toMap().value("id") == source.value("id")) {
            emit notice(tr("插件源已存在"), true);
            return false;
        }
    m_catalogLoading = true;
    emit changed();
    const auto generation = ++m_generation;
    fetchSource(source, generation, [this, generation](QVariantMap source, QVariantList plugins) {
        if (generation != m_generation)
            return;
        m_catalogLoading = false;
        if (!source.value("lastError").toString().isEmpty()) {
            if (source.value("id") == m_shareTarget.value("sourceId"))
                m_shareInstallError = source.value("lastError").toString();
            emit notice(source.value("lastError").toString(), true);
            emit sourceAdded(false);
        } else {
            m_sources.append(source);
            m_entries.append(plugins);
            save();
            emit notice(tr("插件源已添加"), false);
            emit sourceAdded(true);
        }
        rebuild();
    });
    return true;
}
bool PluginManager::patchSource(const QString &id, const QString &name, bool enabled)
{
    if (m_catalogLoading || m_busy || m_closing)
        return false;
    for (auto &value : m_sources) {
        auto source = value.toMap();
        if (source.value("id") != id)
            continue;
        if (!name.trimmed().isEmpty())
            source["name"] = name.trimmed().left(128);
        source["enabled"] = enabled;
        value = source;
        m_sourceFilter.clear();
        save();
        rebuild();
        if (enabled)
            refreshCatalog(true);
        return true;
    }
    return false;
}
bool PluginManager::removeSource(const QString &id)
{
    if (m_catalogLoading || m_busy || m_closing)
        return false;
    for (int i = 0; i < m_sources.size(); ++i)
        if (m_sources[i].toMap().value("id") == id) {
            if (m_sources[i].toMap().value("official").toBool())
                return false;
            m_sources.removeAt(i);
            for (int j = int(m_entries.size()) - 1; j >= 0; --j)
                if (m_entries[j].toMap().value("sourceId") == id)
                    m_entries.removeAt(j);
            m_sourceFilter.clear();
            save();
            rebuild();
            return true;
        }
    return false;
}
QVariantMap PluginManager::entry(const QString &key) const
{
    for (const auto &value : m_entries)
        if (value.toMap().value("key") == key)
            return value.toMap();
    return {};
}
bool PluginManager::install(const QString &key)
{
    if (m_busy || m_catalogLoading || m_closing || m_runtime.loading())
        return false;
    const auto e = entry(key);
    if (e.isEmpty() || !e.value("compatible").toBool() ||
        !sourceIds().contains(e.value("sourceId").toString()))
        return false;
    const auto current = m_runtime.record(m_runtime.installationForId(e.value("id").toString()));
    const auto origin = current.value("origin").toMap();
    if (!current.isEmpty() &&
        (origin.value("kind") != "marketplace" || origin.value("id") != e.value("sourceId"))) {
        emit notice(tr("插件 ID 来源冲突，请先卸载已有插件"), true);
        return false;
    }
    m_pendingPluginId = e.value("id").toString();
    m_busy = true;
    m_installWasUpdate = !m_runtime.installationForId(e.value("id").toString()).isEmpty();
    m_installingKey = key;
    rebuild();
    get(QUrl(raw({{"repository", e.value("repository")}}, e.value("artifact").toString())),
        128 * 1024 * 1024, [this, e](QByteArray bytes, QString error) {
            if (!error.isEmpty())
                finishInstall(false, error);
            else
                installArchive(bytes, e);
        });
    return true;
}
void PluginManager::installArchive(const QByteArray &bytes, const QVariantMap &entry)
{
    if (!entry.isEmpty() &&
        QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) !=
            entry.value("sha256").toString()) {
        finishInstall(false, tr("插件包 SHA-256 与插件源索引不一致，请刷新后重试"));
        return;
    }
    m_staging = QSharedPointer<QTemporaryDir>::create();
    if (!m_staging->isValid()) {
        finishInstall(false, tr("无法创建安装暂存目录"));
        return;
    }
    QFile file(m_staging->path() + "/package.echomod");
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
        finishInstall(false, tr("无法写入插件包"));
        return;
    }
    file.close();
    extractLocal(file.fileName(), entry);
}
void PluginManager::extractLocal(const QString &archive, const QVariantMap &entry)
{
    if (!m_staging)
        m_staging = QSharedPointer<QTemporaryDir>::create();
    const auto stage = m_staging;
    auto *watcher = new QFutureWatcher<QPair<QString, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, entry, stage] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (m_closing)
            return;
        if (!result.second.isEmpty()) {
            finishInstall(false, result.second);
            return;
        }
        const QVariantMap origin = entry.isEmpty()
                                       ? QVariantMap{{"kind", "local"}, {"name", tr("本地安装")}}
                                       : QVariantMap{{"kind", "marketplace"},
                                                     {"id", entry.value("sourceId")},
                                                     {"name", entry.value("sourceName")},
                                                     {"url", entry.value("sourceUrl")}};
        m_pendingPluginId = localPluginId(result.first);
        if (!m_runtime.installDirectory(result.first, origin, entry.value("manifest").toMap(),
                                        stage))
            finishInstall(false, tr("插件包无效、清单已变更或运行时正忙，请刷新后重试"));
    });
    watcher->setFuture(QtConcurrent::run([archive, stage, entry]() -> QPair<QString, QString> {
        const auto unpacked = stage->path() + "/unpacked";
        if (const auto error = PluginPackage::extractArchive(archive, unpacked); !error.isEmpty())
            return {{}, error};
        QString candidate = unpacked;
        if (!QFileInfo(candidate + "/manifest.json").isFile())
            return {{}, "Archive must contain one plugin manifest"};
        return {candidate, {}};
    }));
}
bool PluginManager::installLocal(const QUrl &url)
{
    if (!url.isLocalFile() || m_busy || m_catalogLoading || m_closing || m_runtime.loading())
        return false;
    const auto path = url.toLocalFile();
    if (QFileInfo(path).isDir()) {
        m_busy = true;
        m_installingKey.clear();
        emit changed();
        m_pendingPluginId = localPluginId(path);
        if (!m_runtime.installDirectory(path, {{"kind", "local"}, {"name", tr("本地安装")}})) {
            finishInstall(false, tr("插件目录无效或不兼容"));
            return true;
        }
        return true;
    }
    if (!path.endsWith(".echomod", Qt::CaseInsensitive)) {
        emit notice(tr("请拖入 .echomod 插件包或开发目录"), true);
        return false;
    }
    m_busy = true;
    m_installingKey.clear();
    emit changed();
    extractLocal(path, {});
    return true;
}
bool PluginManager::installLocalBatch(const QList<QUrl> &urls)
{
    if (urls.isEmpty() || urls.size() > 128 || m_catalogLoading || m_closing)
        return false;
    if (m_localBatch) {
        if (m_localTotal + urls.size() > 128)
            return false;
        // Native file-open events arrive separately, including while the first
        // file is being captured. Keep them in the same bounded install queue.
        m_localQueue.append(urls);
        m_localTotal += int(urls.size());
        rebuild();
        return true;
    }
    if (m_busy)
        return false;
    m_localQueue = urls;
    m_localTotal = int(urls.size());
    m_localDone = 0;
    m_localFailed = false;
    m_localBatch = true;
    startLocalNext();
    return true;
}
void PluginManager::startLocalNext()
{
    if (m_closing)
        return;
    if (m_runtime.loading() || m_catalogLoading) {
        m_busy = true;
        rebuild();
        QTimer::singleShot(25, this, &PluginManager::startLocalNext);
        return;
    }
    m_busy = false;
    const auto next = m_localQueue.takeFirst();
    if (!installLocal(next))
        finishInstall(false, tr("无法安装所拖入的插件，请使用 .echomod 插件包或开发目录"));
}
void PluginManager::finishInstall(bool success, const QString &error)
{
    const auto selected = entry(m_installingKey);
    if (!selected.isEmpty())
        reportInstallation(selected, success);
    m_busy = false;
    m_installingKey.clear();
    m_staging.clear();
    m_pendingPluginId.clear();
    m_pendingInstallation.clear();
    if (m_shareInstalling) {
        m_shareInstalling = false;
        m_shareInstallError = success ? QString() : error;
        resolveShared();
        if (success)
            emit sharedInstalled();
    }
    if (m_localBatch) {
        ++m_localDone;
        m_localFailed = m_localFailed || !success;
        if (!success)
            emit notice(error, true);
        if (!m_localQueue.isEmpty() && !m_closing) {
            m_busy = true;
            rebuild();
            QTimer::singleShot(0, this, &PluginManager::startLocalNext);
            return;
        }
        m_localBatch = false;
        rebuild();
        emit notice(m_localFailed ? tr("批量安装完成，部分插件安装失败") : tr("插件安装完成"),
                    m_localFailed);
        return;
    }
    m_updateFailed = m_updateFailed || !success;
    if (m_batch)
        ++m_batchDone;
    rebuild();
    if (!m_updateQueue.isEmpty() && !m_closing) {
        m_busy = true;
        m_installingKey = m_updateQueue.first();
        rebuild();
        QTimer::singleShot(0, this, &PluginManager::startUpdateNext);
        return;
    }
    const bool wasBatch = m_batch;
    m_batch = false;
    emit changed();
    emit notice(success && !m_updateFailed ? wasBatch ? tr("插件全部更新完成") : tr("插件安装完成")
                : error.isEmpty()          ? tr("部分插件更新失败")
                                           : error,
                !success || m_updateFailed);
    m_updateFailed = false;
}
void PluginManager::startUpdateNext()
{
    if (m_closing || m_updateQueue.isEmpty())
        return;
    if (m_runtime.loading()) {
        QTimer::singleShot(25, this, &PluginManager::startUpdateNext);
        return;
    }
    const auto next = m_updateQueue.takeFirst();
    m_busy = false;
    if (!install(next))
        finishInstall(false, tr("插件更新未能开始"));
}
void PluginManager::updateAll()
{
    if (m_busy || m_catalogLoading || m_closing)
        return;
    QSet<QString> ids;
    for (const auto &value : m_entries) {
        const auto e = value.toMap();
        auto current = m_runtime.record(m_runtime.installationForId(e.value("id").toString()));
        if (!current.isEmpty() && e.value("compatible").toBool() &&
            sourceIds().contains(e.value("sourceId").toString()) &&
            QVersionNumber::fromString(e.value("version").toString()) >
                QVersionNumber::fromString(current.value("version").toString()) &&
            !ids.contains(e.value("id").toString())) {
            m_updateQueue.append(e.value("key").toString());
            ids.insert(e.value("id").toString());
        }
    }
    if (!m_updateQueue.isEmpty()) {
        m_batch = true;
        m_batchTotal = int(m_updateQueue.size());
        m_batchDone = 0;
        const auto next = m_updateQueue.takeFirst();
        if (!install(next))
            finishInstall(false, tr("插件更新未能开始"));
    }
}
bool PluginManager::openDirectory()
{
    if (!QDir().mkpath(m_runtime.directory()))
        return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(m_runtime.directory()));
}
bool PluginManager::openRepository(const QString &key)
{
    const auto url = entry(key).value("sourceUrl").toString();
    return !url.isEmpty() && QDesktopServices::openUrl(QUrl(url));
}
void PluginManager::attachSharing(SharingCommands &sharing)
{
    m_sharing = &sharing;
}
void PluginManager::share(const QString &key)
{
    const auto e = entry(key);
    if (e.isEmpty())
        return;
    if (m_sharing) {
        m_sharing->copyTarget({ShareResource::Plugin,
                               e.value("id").toString(),
                               {{"source", e.value("sourceUrl").toString()},
                                {"version", e.value("version").toString()},
                                {"path", e.value("path").toString()}}},
                              e.value("name").toString());
        return;
    }
    QUrl url("echomusicnext://plugin");
    QUrlQuery query;
    query.addQueryItem("source", e.value("sourceUrl").toString());
    query.addQueryItem("id", e.value("id").toString());
    query.addQueryItem("version", e.value("version").toString());
    query.addQueryItem("path", e.value("path").toString());
    url.setQuery(query);
    QGuiApplication::clipboard()->setText(url.toString());
    emit notice(tr("插件分享链接已复制"), false);
}
bool PluginManager::openShare(const ShareTarget &target)
{
    if (target.type != ShareResource::Plugin || !validShareTarget(target))
        return false;
    // The existing manager remains the owner of source approval, manifest
    // checks and installation. Encode each value once at this typed boundary.
    QByteArray query = "id=" + QUrl::toPercentEncoding(target.id);
    for (const auto &key : {"source", "version", "path"})
        query += '&' + QByteArray(key) + '=' + QUrl::toPercentEncoding(target.query.value(key));
    QUrl url("echomusicnext://plugin");
    url.setQuery(QString::fromLatin1(query), QUrl::StrictMode);
    return openShare(url);
}
bool PluginManager::openShare(const QUrl &url)
{
    if (m_closing || m_busy || m_catalogLoading || !url.isValid() ||
        url.toEncoded().size() > 8192 || url.scheme() != "echomusicnext" ||
        url.host() != "plugin" || !url.userInfo().isEmpty() || url.port() != -1 ||
        url.hasFragment() || (!url.path().isEmpty() && url.path() != "/"))
        return false;
    const QUrlQuery query(url);
    QSet<QString> names;
    for (const auto &item : query.queryItems()) {
        if (!QStringList{"source", "id", "version", "path"}.contains(item.first) ||
            names.contains(item.first))
            return false;
        names.insert(item.first);
    }
    const auto source = normalizeSource(query.queryItemValue("source", QUrl::FullyDecoded), {});
    const auto id = query.queryItemValue("id", QUrl::FullyDecoded);
    const auto path = query.queryItemValue("path", QUrl::FullyDecoded);
    const auto version = query.queryItemValue("version", QUrl::FullyDecoded);
    if (source.isEmpty() || !QRegularExpression("^[a-z][a-z0-9.-]{1,127}$").match(id).hasMatch() ||
        !safePath(path) ||
        !QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$").match(version).hasMatch())
        return false;
    m_shareTarget = {{"sourceId", source.value("id")},
                     {"sourceUrl", source.value("url")},
                     {"id", id},
                     {"path", path},
                     {"version", version}};
    m_shareRecord.clear();
    m_shareError.clear();
    m_shareInstallError.clear();
    m_shareState = "loading";
    emit shareOpened();
    bool enabled = false;
    for (const auto &value : m_sources)
        if (value.toMap().value("id") == source.value("id"))
            enabled = value.toMap().value("enabled").toBool();
    if (enabled)
        refreshCatalog(true);
    resolveShared();
    emit changed();
    return true;
}
void PluginManager::resolveShared()
{
    if (m_shareTarget.isEmpty() || m_closing)
        return;
    QString state, error;
    QVariantMap record, source;
    for (const auto &value : m_sources)
        if (value.toMap().value("id") == m_shareTarget.value("sourceId"))
            source = value.toMap();
    if (!m_shareInstallError.isEmpty()) {
        state = "failed";
        error = m_shareInstallError;
    } else if (m_catalogLoading)
        state = "loading";
    else if (source.isEmpty() || !source.value("enabled").toBool())
        state = "need-source";
    else if (!source.value("lastError").toString().isEmpty()) {
        state = "failed";
        error = source.value("lastError").toString();
    } else {
        for (const auto &value : m_entries) {
            const auto candidate = value.toMap();
            if (candidate.value("sourceId") == m_shareTarget.value("sourceId") &&
                candidate.value("id") == m_shareTarget.value("id") &&
                candidate.value("path") == m_shareTarget.value("path")) {
                record = candidate;
                break;
            }
        }
        if (record.isEmpty()) {
            state = "failed";
            error = tr("插件源中未找到分享的插件");
        } else if (!record.value("compatible").toBool()) {
            state = "failed";
            error = record.value("error").toString();
        } else if (QVersionNumber::fromString(record.value("version").toString()) <
                   QVersionNumber::fromString(m_shareTarget.value("version").toString())) {
            state = "failed";
            error = tr("插件源版本低于分享版本，请刷新后重试");
        } else {
            record["sharedVersion"] = m_shareTarget.value("version");
            const auto current =
                m_runtime.record(m_runtime.installationForId(record.value("id").toString()));
            state = !current.isEmpty() &&
                            QVersionNumber::fromString(current.value("version").toString()) >=
                                QVersionNumber::fromString(record.value("version").toString())
                        ? "installed"
                        : "ready";
            record["updateAvailable"] = !current.isEmpty();
        }
    }
    if (state != m_shareState || error != m_shareError || record != m_shareRecord) {
        m_shareState = state;
        m_shareError = error;
        m_shareRecord = record;
        emit changed();
    }
}
bool PluginManager::acceptShareSource()
{
    if (m_shareState != "need-source" || m_busy || m_catalogLoading)
        return false;
    for (const auto &value : m_sources) {
        const auto source = value.toMap();
        if (source.value("id") == m_shareTarget.value("sourceId"))
            return patchSource(source.value("id").toString(), source.value("name").toString(),
                               true);
    }
    return addSource(shareSourceUrl(), {});
}
bool PluginManager::installShared()
{
    if (m_shareState != "ready")
        return false;
    m_shareInstalling = true;
    if (install(m_shareRecord.value("key").toString()))
        return true;
    m_shareInstalling = false;
    return false;
}
EchoPluginContext *PluginManager::requestSettings(const QString &installation)
{
    const auto record = m_runtime.record(installation);
    auto *scope = m_runtime.context(installation);
    if (scope && scope->active() && !scope->ui()->settingsComponent().isEmpty())
        return scope;
    const auto message =
        !record.value("compatible").toBool() ? tr("插件与当前主程序版本不兼容，无法加载设置")
        : !record.value("enabled").toBool()  ? tr("启用插件后才能读取它提供的设置项")
        : m_runtime.safeMode()               ? tr("安全模式下不会加载插件设置")
        : record.value("status").toString() != tr("运行中") ? tr("插件运行后才能读取它提供的设置项")
                                                            : tr("该插件没有提供设置项");
    const bool warning = !record.value("compatible").toBool() ||
                         !record.value("enabled").toBool() || m_runtime.safeMode() ||
                         record.value("status").toString() != tr("运行中");
    emit notice(message, warning, !warning);
    return nullptr;
}
void PluginManager::openDocumentation()
{
    QDesktopServices::openUrl(QUrl(official + "/blob/main/docs/sdk.md"));
}

QString PluginManager::updateAllLabel() const
{
    return m_batch ? tr("更新中 %1/%2").arg(m_batchDone).arg(m_batchTotal)
                   : tr("更新全部 %1 个插件").arg(updateCount());
}
void PluginManager::refreshStatistics(bool force)
{
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (m_closing || m_statsLoading || now < m_statsRetryAt ||
        (!force && now - m_statsAt < 10 * 60 * 1000))
        return;
    QJsonArray plugins;
    QSet<QString> requested;
    for (const auto &value : m_entries) {
        const auto e = value.toMap();
        if (!sourceIds().contains(e.value("sourceId").toString()))
            continue;
        requested.insert(e.value("key").toString());
        plugins.append(QJsonObject{{"sourceId", e.value("sourceId").toString()},
                                   {"pluginId", e.value("id").toString()}});
    }
    if (plugins.isEmpty())
        return;
    m_statsLoading = true;
    const auto generation = m_generation;
    get(
        QUrl(statisticsEndpoint + "/v1/plugins/stats"), 1024 * 1024,
        [this, requested, generation](QByteArray bytes, QString error) {
            m_statsLoading = false;
            if (generation != m_generation)
                return;
            QVariantMap results;
            if (error.isEmpty())
                results = PluginMarketplaceStats::parse(bytes, requested, error);
            const auto now = QDateTime::currentMSecsSinceEpoch();
            if (!error.isEmpty()) {
                m_statsFailures = std::min(6, m_statsFailures + 1);
                m_statsRetryAt = now + std::min(30 * 60 * 1000, 30000 * (1 << m_statsFailures));
            } else {
                for (auto it = results.begin(); it != results.end(); ++it)
                    m_stats.insert(it.key(), it.value());
                m_statsAt = now;
                m_statsRetryAt = 0;
                m_statsFailures = 0;
            }
            save();
            rebuild();
        },
        QJsonDocument(QJsonObject{{"plugins", plugins}}).toJson(QJsonDocument::Compact), 10000);
}
void PluginManager::reportInstallation(const QVariantMap &record, bool success)
{
    const QString event = success ? m_installWasUpdate ? "update" : "install" : "failure";
    QJsonObject plugin{{"sourceId", record.value("sourceId").toString()},
                       {"pluginId", record.value("id").toString()},
                       {"version", record.value("version").toString()},
                       {"sourceUrl", record.value("sourceUrl").toString()},
                       {"sourceName", record.value("repository").toString()},
                       {"repo", record.value("sourceUrl").toString()},
                       {"packagePath", record.value("path").toString()}};
    // Report canonical repository metadata, never user-defined source labels,
    // account/device/activation identity or private paths.
    get(
        QUrl(statisticsEndpoint + "/v1/plugins/events"), 1024 * 1024,
        [this](QByteArray, QString error) {
            if (error.isEmpty())
                refreshStatistics(true);
        },
        QJsonDocument(QJsonObject{{"event", event},
                                  {"plugin", plugin},
                                  {"error", success ? "" : "InstallFailed"}})
            .toJson(QJsonDocument::Compact),
        10000);
}
void PluginManager::shutdown()
{
    if (m_closing)
        return;
    m_closing = true;
    ++m_generation;
    const auto replies = m_replies;
    for (auto *reply : replies)
        reply->abort();
    m_updateQueue.clear();
    m_localQueue.clear();
}
} // namespace EchoMusic
