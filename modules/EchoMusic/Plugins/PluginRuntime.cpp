#include "PluginRuntime.h"
#include "PluginDataTransaction.h"
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QThread>
#include <QDateTime>
#include <utility>
namespace EchoMusic
{
namespace
{
QString identityKey(const QString &id, const QVariantMap &origin)
{
    return origin.value("kind") == "marketplace"
               ? "plugins/identities/" + origin.value("id").toString() + '/' + id
               : "plugins/id/" + id;
}
bool internalDirectory(const QString &name)
{
    return QStringList{"packages", "data", "transactions"}.contains(name, Qt::CaseInsensitive);
}
// Only a direct, non-symlink child belongs to the root's manual-install area.
// External development sources and internal stores must never be deleted here.
QString rootSource(const QString &source, const QString &root)
{
    const QFileInfo info(source);
    const auto canonicalRoot = QFileInfo(root).canonicalFilePath();
    return !canonicalRoot.isEmpty() &&
                   (info.isDir() || (info.isFile() &&
                                     info.suffix().compare("echomod", Qt::CaseInsensitive) == 0)) &&
                   !info.isSymLink() && !internalDirectory(info.fileName()) &&
                   info.dir().canonicalPath() == canonicalRoot
               ? info.canonicalFilePath()
               : QString();
}
} // namespace
void PluginSettlement::resolved()
{
    if (std::exchange(m_done, true))
        return;
    timeout.stop();
    auto callback = std::exchange(complete, {});
    if (callback)
        callback({});
    deleteLater();
}
void PluginSettlement::rejected(const QString &message)
{
    if (std::exchange(m_done, true))
        return;
    timeout.stop();
    auto callback = std::exchange(complete, {});
    if (callback)
        callback(message.isEmpty() ? "Plugin Promise rejected" : message);
    deleteLater();
}
void PluginSettlement::invalidate()
{
    m_done = true;
    timeout.stop();
    complete = {};
    deleteLater();
}
PluginRuntime::PluginRuntime(QQmlEngine &engine, QSettings &preferences, PluginHost host,
                             QString root, bool safeMode)
    : m_engine(engine), m_preferences(preferences), m_host(host), m_root(std::move(root)),
      m_safeMode(safeMode || preferences.value("plugins/safeMode", false).toBool())
{
    connect(this, &PluginRuntime::loadingChanged, this, &PluginRuntime::advanceRefresh,
            Qt::QueuedConnection);
}
PluginRuntime::~PluginRuntime()
{
    Q_ASSERT(m_imports == 0);
    for (const auto &entry : m_entries)
        Q_ASSERT(!entry->context);
}
int PluginRuntime::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}
QVariant PluginRuntime::data(const QModelIndex &item, int role) const
{
    if (!item.isValid() || item.row() < 0 || item.row() >= rowCount())
        return {};
    const auto &entry = *m_entries[size_t(item.row())];
    switch (role) {
    case IdRole:
        return entry.package.id;
    case NameRole:
        return entry.package.name;
    case VersionRole:
        return entry.package.version;
    case StateRole:
        return int(entry.state);
    case EnabledRole:
        return entry.enabled;
    case ErrorRole:
        return entry.error;
    case InstallationRole:
        return entry.package.installationId;
    case RecordRole:
        return record(entry.package.installationId);
    default:
        return {};
    }
}
QHash<int, QByteArray> PluginRuntime::roleNames() const
{
    return {{IdRole, "pluginId"},
            {NameRole, "pluginName"},
            {VersionRole, "pluginVersion"},
            {StateRole, "pluginState"},
            {EnabledRole, "pluginEnabled"},
            {ErrorRole, "pluginError"},
            {InstallationRole, "installationId"},
            {RecordRole, "pluginRecord"}};
}
PluginRuntime::Entry *PluginRuntime::find(const QString &installation) const
{
    for (const auto &entry : m_entries)
        if (entry->package.installationId == installation)
            return entry.get();
    return nullptr;
}
EchoPluginContext *PluginRuntime::context(const QString &installation) const
{
    auto *entry = find(installation);
    return entry ? entry->context.data() : nullptr;
}
PluginRuntime::State PluginRuntime::state(const QString &installation) const
{
    auto *entry = find(installation);
    return entry ? entry->state : Invalid;
}
bool PluginRuntime::loading() const
{
    if (m_reconcileQueued && !m_closing && !m_safeMode && !m_maintenance)
        return true;
    if (m_imports || m_refreshing || m_startScanPending)
        return true;
    for (const auto &entry : m_entries)
        if (entry->retirementPending || entry->state == Discovered || entry->state == Activating ||
            entry->state == Deactivating)
            return true;
    return false;
}
void PluginRuntime::changed(Entry &entry)
{
    queueReconcile();
    emit catalogChanged();
    if (!entry.error.isEmpty() && entry.failure.value("message").toString() != entry.error) {
        entry.failure = {{"message", entry.error},
                         {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                         {"reason", entry.state == Incompatible ? "不兼容"
                                    : entry.state == Blocked    ? "依赖未就绪"
                                                                : "运行异常"}};
        m_preferences.setValue("plugins/failures/" + entry.package.installationId, entry.failure);
    }
    if (!std::exchange(m_loadingNotification, true)) {
        QTimer::singleShot(0, this, [this] {
            m_loadingNotification = false;
            emit loadingChanged();
        });
    }
    for (size_t i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].get() == &entry) {
            emit dataChanged(index(int(i)), index(int(i)));
            break;
        }
}
void PluginRuntime::persist()
{
    QVariantList catalog;
    for (const auto &entry : m_entries)
        if (!(entry->mutation == Entry::Updating && entry->newInstall))
            catalog.append(
                QVariantMap{{"source", entry->mutation == Entry::Updating && !entry->newInstall
                                           ? entry->previousSource
                                           : entry->source},
                            {"installation", entry->package.installationId},
                            {"root", entry->mutation == Entry::Updating && !entry->newInstall
                                         ? entry->previousPackage.root
                                         : entry->package.root},
                            {"origin", entry->mutation == Entry::Updating && !entry->newInstall
                                           ? entry->previousOrigin
                                           : entry->origin},
                            {"enabled", entry->enabled}});
    m_preferences.setValue("plugins/installations", catalog);
    m_preferences.sync();
}
void PluginRuntime::start(const QStringList &imports)
{
    if (std::exchange(m_started, true))
        return;
    if (!m_engine.property("echoPluginSdkRegistered").toBool()) {
        // importModule accepts filesystem/QResource names, not qrc: URL text.
        // Do not silently run plugins with an unavailable native SDK module.
        const auto sdk = m_engine.importModule(":/echo/plugins/sdk.mjs");
        if (sdk.isError() || !sdk.property("awaitOperation").isCallable())
            m_sdkError = "Cannot load PluginSdk Promise adapter";
        else {
            auto exports = m_engine.newObject();
            exports.setProperty("awaitOperation", sdk.property("awaitOperation"));
            exports.setProperty("pluginService", sdk.property("pluginService"));
            if (!m_engine.registerModule("echo:plugin-sdk", exports))
                m_sdkError = "Cannot register PluginSdk module";
            else
                m_engine.setProperty("echoPluginSdkRegistered", true);
        }
        if (m_engine.hasError()) {
            m_sdkError += ": " + m_engine.catchError().toString();
        }
    }
    QStringList normalizedImports;
    for (const auto &source : imports) {
        const auto canonical = QFileInfo(source).canonicalFilePath();
        const auto identity =
            canonical.isEmpty() ? QFileInfo(source).absoluteFilePath() : canonical;
        if (!normalizedImports.contains(identity))
            normalizedImports.append(identity);
    }
    const auto catalog = m_preferences.value("plugins/installations").toList();
    for (const auto &value : catalog) {
        const auto record = value.toMap();
        const auto source = record.value("source").toString();
        if (normalizedImports.contains(source))
            continue;
        importResolved(record.value("root").toString(), source,
                       record.value("installation").toString(), record.value("enabled").toBool());
        if (auto *entry = find(record.value("installation").toString()))
            entry->origin = record.value("origin").toMap();
    }
    for (const auto &source : normalizedImports)
        importDirectory(source, true);
    m_startScanPending = true;
    QTimer::singleShot(0, this, &PluginRuntime::advanceRefresh);
}
void PluginRuntime::importDirectory(const QString &source, bool enabled)
{
    if (m_closing || m_maintenance)
        return;
    const auto canonical = QFileInfo(source).canonicalFilePath();
    const auto identity = canonical.isEmpty() ? QFileInfo(source).absoluteFilePath() : canonical;
    const auto settingsKey =
        "plugins/source/" +
        QString::fromLatin1(
            QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
    auto installation = m_preferences.value(settingsKey).toString();
    if (installation.isEmpty()) {
        installation = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_preferences.setValue(settingsKey, installation);
    }
    importResolved(identity, identity, installation, enabled);
    if (auto *entry = find(installation)) {
        if (const auto owned = rootSource(source, m_root); !owned.isEmpty())
            entry->origin = {{"kind", "local"},
                             {QFileInfo(owned).isFile() ? "file" : "directory", owned}};
    }
}
void PluginRuntime::importResolved(const QString &directory, const QString &source,
                                   const QString &installation, bool enabled)
{
    if (m_closing || installation.isEmpty())
        return;
    if (find(installation))
        return;
    const int row = rowCount();
    beginInsertRows({}, row, row);
    auto entry = std::make_unique<Entry>();
    entry->source = source;
    entry->failure = m_preferences.value("plugins/failures/" + installation).toMap();
    entry->enabled = enabled;
    entry->package.installationId = installation;
    auto *target = entry.get();
    m_entries.push_back(std::move(entry));
    endInsertRows();
    load(*target, directory);
}
void PluginRuntime::load(Entry &entry, const QString &directory)
{
    entry.state = Discovered;
    changed(entry);
    auto *target = &entry;
    const auto installation = entry.package.installationId;
    const bool recover = m_preferences.value("plugins/rollback/" + installation, false).toBool();
    ++m_imports;
    auto *watcher = new QFutureWatcher<PluginPackage>(this);
    connect(watcher, &QFutureWatcher<PluginPackage>::finished, this,
            [this, watcher, target, recover] {
                --m_imports;
                auto package = watcher->result();
                // Startup restoration retains the original installation identity.
                package.installationId = target->package.installationId;
                target->package = std::move(package);
                watcher->deleteLater();
                target->error = target->package.error;
                if (target->error.isEmpty() && !m_sdkError.isEmpty())
                    target->error = m_sdkError;
                target->state = target->error.isEmpty()        ? Disabled
                                : target->package.incompatible ? Incompatible
                                                               : Invalid;
                if (target->error.isEmpty()) {
                    for (const auto &other : m_entries)
                        if (other.get() != target && other->package.id == target->package.id &&
                            other->state != Invalid && other->state != Incompatible) {
                            target->state = Incompatible;
                            target->error = "Another installation owns this plugin ID";
                        }
                }
                if (recover && target->error.isEmpty()) {
                    m_preferences.remove("plugins/rollback/" + target->package.installationId);
                }
                persist();
                if (recover && target->error.isEmpty()) {
                    const auto backup = m_root + "/transactions/" + target->package.installationId;
                    discardSnapshot(backup);
                }
                changed(*target);
                queueReconcile();
                checkClosed();
            });
    const auto packages = m_root + "/packages";
    watcher->setFuture(
        QtConcurrent::run([directory, packages, installation, recover, root = m_root] {
            if (recover) {
                const auto error = PluginDataTransaction::restore(
                    root + "/data/" + installation, root + "/transactions/" + installation);
                if (!error.isEmpty()) {
                    PluginPackage failed;
                    failed.installationId = installation;
                    failed.error = error;
                    return failed;
                }
            }
            return PluginPackage::importDirectory(directory, packages, installation);
        }));
}
bool PluginRuntime::setEnabled(const QString &installation, bool enabled)
{
    auto *entry = find(installation);
    if (!entry || m_closing || m_maintenance || (enabled && m_imports) ||
        m_preferences.value("plugins/rollback/" + installation, false).toBool() ||
        entry->state == Discovered || entry->state == Invalid || entry->state == Incompatible ||
        entry->mutation != Entry::None || entry->state == Deactivating ||
        (enabled && entry->state == Activating))
        return false;
    entry->enabled = enabled;
    if (!enabled && !entry->context && entry->state == Blocked) {
        entry->state = Disabled;
        entry->error.clear();
    }
    persist();
    changed(*entry);
    if (!enabled && entry->context)
        retire(*entry, Disabled);
    else if (enabled && !m_safeMode && !entry->context) {
        if (entry->state == Failed) {
            entry->state = Disabled;
            entry->error.clear();
            changed(*entry);
        }
        queueReconcile();
    }
    return true;
}
bool PluginRuntime::reload(const QString &installation)
{
    auto *entry = find(installation);
    if (!entry || m_closing || m_maintenance || m_imports ||
        m_preferences.value("plugins/rollback/" + installation, false).toBool() ||
        entry->state == Discovered || entry->state == Deactivating || entry->state == Activating ||
        entry->mutation != Entry::None)
        return false;
    if (entry->context) {
        entry->reloadPending = true;
        retire(*entry, Disabled);
    } else
        load(*entry, QFileInfo::exists(entry->source) ? entry->source : entry->package.root);
    return true;
}
bool PluginRuntime::retry(const QString &installation)
{
    auto *entry = find(installation);
    if (!entry || m_closing || m_safeMode || entry->context ||
        (entry->state != Failed && entry->state != Blocked))
        return false;
    return setEnabled(installation, true);
}
EchoPluginOperation *PluginRuntime::executeCommand(const QString &installation, const QString &id,
                                                   const QVariantMap &arguments)
{
    auto *entry = find(installation);
    return entry && entry->state == Active && entry->context
               ? entry->context->executeCommand(id, arguments)
               : nullptr;
}
void PluginRuntime::settle(Entry &entry, const QJSValue &value,
                           std::function<void(QString)> complete)
{
    auto *bridge = new PluginSettlement(this);
    QQmlEngine::setObjectOwnership(bridge, QQmlEngine::CppOwnership);
    entry.settlement = bridge;
    bridge->complete = std::move(complete);
    bridge->timeout.setSingleShot(true);
    bridge->timeout.setInterval(5000);
    connect(&bridge->timeout, &QTimer::timeout, bridge,
            [bridge] { bridge->rejected("Plugin lifecycle Promise timed out (5 s)"); });
    bridge->timeout.start();
    const auto attach = m_engine.evaluate(
        "(function(value, bridge) { Promise.resolve(value).then(function() { try { "
        "bridge.resolved(); } catch(ignored) {} }, "
        "function(error) { try { bridge.rejected(String(error)); } catch(ignored) {} }); })");
    auto result = attach.call({value, m_engine.newQObject(bridge)});
    if (m_engine.hasError()) {
        result = m_engine.catchError();
        if (!result.isError())
            result = m_engine.newErrorObject(QJSValue::GenericError, result.toString());
    }
    if (result.isError())
        bridge->rejected(result.toString());
}
PluginDependencyGraph PluginRuntime::dependencyPlan(const PluginPackage *replacement) const
{
    QList<PluginPackage> packages;
    QSet<QString> enabled;
    bool replaced = false;
    for (const auto &entry : m_entries) {
        const bool replace = replacement && entry->package.id == replacement->id;
        packages.append(replace ? *replacement : entry->package);
        replaced |= replace;
        if (entry->enabled)
            enabled.insert(entry->package.id);
    }
    if (replacement && !replaced) {
        packages.append(*replacement);
        enabled.insert(replacement->id);
    }
    return PluginDependencyGraph::resolve(packages, enabled);
}
bool PluginRuntime::dependsOn(const Entry &consumer, const QString &provider) const
{
    return consumer.package.dependencies.contains(provider) ||
           consumer.package.optionalDependencies.contains(provider);
}
void PluginRuntime::queueReconcile()
{
    if (std::exchange(m_reconcileQueued, true))
        return;
    QTimer::singleShot(0, this, [this] {
        m_reconcileQueued = false;
        advanceRetirements();
        reconcile();
        emit loadingChanged();
    });
}
void PluginRuntime::reconcile()
{
    if (m_closing || m_maintenance || m_safeMode || m_imports || m_startScanPending)
        return;
    const auto plan = dependencyPlan();
    for (const auto &entry : m_entries) {
        if (entry->enabled && entry->context && plan.errors.contains(entry->package.id))
            retire(*entry, Blocked, plan.errors.value(entry->package.id));
        if (entry->enabled && !entry->context && entry->state != Failed &&
            entry->state != Invalid && entry->state != Incompatible &&
            plan.errors.contains(entry->package.id)) {
            const auto error = plan.errors.value(entry->package.id);
            if (entry->state != Blocked || entry->error != error) {
                entry->state = Blocked;
                entry->error = error;
                changed(*entry);
            }
            if (entry->mutation != Entry::None && entry->applying)
                finishMutation(*entry);
        }
    }
    for (const auto &id : plan.order) {
        auto *entry = find(installationForId(id));
        if (!entry || !entry->enabled || plan.errors.contains(id) || entry->retirementPending)
            continue;
        if (entry->state == Activating || entry->state == Deactivating)
            return; // Deterministic ordered activation, including async activate.
        if (!entry->context && (entry->state == Disabled || entry->state == Blocked)) {
            activate(*entry);
            if (entry->state == Activating)
                return;
        }
    }
}
void PluginRuntime::activate(Entry &entry)
{
    if (m_closing || m_maintenance || m_safeMode || entry.context || entry.retirementPending)
        return;
    if (m_imports || m_startScanPending) {
        queueReconcile();
        return;
    }
    // All activation requests pass through reconcile's graph preflight.
    QString error;
    for (const auto &id :
         entry.package.dependencies.keys() + entry.package.optionalDependencies.keys()) {
        auto *provider = find(installationForId(id));
        if (provider && (provider->retirementPending || provider->state == Deactivating))
            error = tr("等待依赖插件 %1 完成排空").arg(id);
    }
    auto dependencies = entry.package.dependencies;
    for (auto it = entry.package.optionalDependencies.begin();
         it != entry.package.optionalDependencies.end(); ++it) {
        auto *provider = find(installationForId(it.key()));
        if (provider && provider->enabled)
            dependencies.insert(it.key(), it.value());
    }
    for (auto it = dependencies.begin(); it != dependencies.end() && error.isEmpty(); ++it) {
        auto *provider = find(installationForId(it.key()));
        if (!provider || provider->state != Active || provider->retirementPending)
            error = tr("等待依赖插件 %1 就绪").arg(it.key());
    }
    if (!error.isEmpty()) {
        if (entry.state != Blocked || entry.error != error) {
            entry.state = Blocked;
            entry.error = error;
            changed(entry);
        }
        if (entry.mutation != Entry::None && entry.applying)
            finishMutation(entry);
        return;
    }
    entry.state = Activating;
    entry.error.clear();
    entry.context = new EchoPluginContext(entry.package, ++m_generation, m_engine, m_host,
                                          m_root + "/data/" + entry.package.installationId, this);
    entry.context->plugins()->setResolver([this](const QString &id) -> EchoPluginContext * {
        auto *provider = find(installationForId(id));
        return provider && provider->state == Active && !provider->retirementPending
                   ? provider->context.data()
                   : nullptr;
    });
    connect(this, &PluginRuntime::catalogChanged, entry.context,
            [scope = QPointer<EchoPluginContext>(entry.context)] {
                if (scope && scope->active())
                    emit scope->plugins()->availabilityChanged();
            });
    connect(entry.context, &EchoPluginContext::callbackFailed, this,
            [this, target = &entry](const QString &message) {
                target->failure = {{"message", message},
                                   {"reason", tr("回调异常")},
                                   {"time", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
                m_preferences.setValue("plugins/failures/" + target->package.installationId,
                                       target->failure);
                m_preferences.sync();
                changed(*target);
                emit pluginFailed(target->package.id, message);
            });
    changed(entry);
    entry.module = m_engine.importModule(entry.package.resolve(entry.package.entry));
    if (m_engine.hasError()) {
        entry.module = m_engine.catchError();
        if (!entry.module.isError())
            entry.module = m_engine.newErrorObject(QJSValue::GenericError, entry.module.toString());
    }
    if (entry.module.isError() || !entry.module.property("activate").isCallable()) {
        retire(entry, Failed,
               entry.module.isError() ? entry.module.toString()
                                      : "Module must export activate(context)");
        return;
    }
    auto result = entry.context->call(entry.module.property("activate"),
                                      {m_engine.newQObject(entry.context)});
    if (m_engine.hasError()) {
        result = m_engine.catchError();
        if (!result.isError())
            result = m_engine.newErrorObject(QJSValue::GenericError, result.toString());
    }
    if (result.isError()) {
        retire(entry, Failed, result.toString());
        return;
    }
    if (!entry.context || entry.state != Activating)
        return;
    settle(entry, result, [this, target = &entry](QString error) {
        if (target->state != Activating || !target->context)
            return;
        if (!error.isEmpty() || !target->context->commit()) {
            retire(*target, Failed, error.isEmpty() ? "Contribution commit failed" : error);
            return;
        }
        target->state = Active;
        changed(*target);
        finishMutation(*target);
    });
}
void PluginRuntime::retire(Entry &entry, State terminal, QString error)
{
    if (entry.state == Deactivating) {
        if (terminal == Failed) {
            entry.afterRetire = terminal;
            entry.error = std::move(error);
        }
        return;
    }
    if (!entry.context)
        return;
    // Consumers drain before their provider. Their enabled preferences remain
    // intact, so an updated/re-enabled provider can reactivate the graph.
    entry.retirementPending = true;
    entry.afterRetire = terminal;
    entry.error = error;
    for (const auto &other : m_entries)
        if (other.get() != &entry && other->context && dependsOn(*other, entry.package.id) &&
            other->state != Deactivating && !other->retirementPending)
            retire(*other, Blocked, tr("依赖插件 %1 正在停用").arg(entry.package.id));
    for (const auto &other : m_entries)
        if (other.get() != &entry && other->context && dependsOn(*other, entry.package.id)) {
            changed(entry);
            return;
        }
    entry.retirementPending = false;
    if (entry.settlement)
        entry.settlement->invalidate();
    entry.state = Deactivating;
    entry.afterRetire = terminal;
    entry.error = std::move(error);
    entry.deactivateDone = false;
    changed(entry);
    connect(entry.context, &EchoPluginContext::resourcesDrained, this,
            [this, target = &entry] { finishRetire(*target); });
    entry.context->beginRetire();
    const auto deactivate = entry.module.property("deactivate");
    if (deactivate.isCallable()) {
        auto result = entry.context->call(deactivate, {m_engine.newQObject(entry.context)});
        if (m_engine.hasError()) {
            result = m_engine.catchError();
            if (!result.isError())
                result = m_engine.newErrorObject(QJSValue::GenericError, result.toString());
        }
        if (result.isError()) {
            emit pluginFailed(entry.package.id, result.toString());
            entry.deactivateDone = true;
        } else
            settle(entry, result, [this, target = &entry](QString error) {
                if (!error.isEmpty())
                    emit pluginFailed(target->package.id, error);
                target->deactivateDone = true;
                finishRetire(*target);
            });
    } else
        entry.deactivateDone = true;
    finishRetire(entry);
}
void PluginRuntime::finishRetire(Entry &entry)
{
    if (entry.state != Deactivating || !entry.deactivateDone || !entry.context ||
        !entry.context->drained())
        return;
    // QML Loader delegate removal and queued completions precede context deletion.
    auto *context = entry.context.data();
    entry.context = nullptr;
    entry.module = {};
    context->deleteLater();
    entry.state = entry.afterRetire;
    changed(entry);
    if (entry.state == Failed)
        emit pluginFailed(entry.package.id, entry.error);
    if (!m_closing && entry.mutation != Entry::None) {
        if (!entry.applying)
            applyMutation(entry);
        else
            finishMutation(entry);
    } else if (!m_closing && std::exchange(entry.reloadPending, false))
        load(entry, entry.source);
    queueReconcile();
    checkMaintenance();
    checkClosed();
}
void PluginRuntime::advanceRetirements()
{
    for (const auto &entry : m_entries)
        if (entry->retirementPending) {
            bool consumers = false;
            for (const auto &other : m_entries)
                consumers |= other.get() != entry.get() && other->context &&
                             dependsOn(*other, entry->package.id);
            if (!consumers)
                retire(*entry, entry->afterRetire, entry->error);
        }
}
int PluginRuntime::enabledCount() const
{
    return int(std::count_if(m_entries.begin(), m_entries.end(),
                             [](const auto &entry) { return entry->enabled; }));
}
QString PluginRuntime::installationForId(const QString &id) const
{
    for (const auto &entry : m_entries)
        if (entry->package.id == id)
            return entry->package.installationId;
    return {};
}
QVariantMap PluginRuntime::record(const QString &installation) const
{
    const auto *entry = find(installation);
    if (!entry)
        return {};
    QString status =
        !entry->enabled                ? tr("已禁用")
        : m_safeMode                   ? tr("安全模式")
        : entry->state == Active       ? tr("运行中")
        : entry->state == Failed       ? tr("出错")
        : entry->state == Invalid      ? tr("无效")
        : entry->state == Incompatible ? tr("不兼容")
        : entry->state == Blocked      ? tr("依赖未就绪")
        : entry->state == Activating || entry->state == Deactivating || entry->state == Discovered
            ? tr("加载中")
            : tr("未运行");
    auto result = entry->package.manifest;
    result.insert("installationId", installation);
    result.insert("status", status);
    result.insert("enabled", entry->enabled);
    result.insert("busy", entry->retirementPending || entry->mutation != Entry::None ||
                              entry->state == Discovered || entry->state == Activating ||
                              entry->state == Deactivating);
    result.insert("compatible", entry->state != Invalid && entry->state != Incompatible);
    result.insert("dependencyBlocked", entry->state == Blocked);
    result.insert("error", entry->error);
    result.insert("failure", entry->failure);
    result.insert("settingsAvailable", entry->context && entry->context->active() &&
                                           !entry->context->ui()->settingsComponent().isEmpty());
    result.insert("sourceName", entry->origin.value("name", tr("本地安装")));
    result.insert("origin", entry->origin);
    result.insert("iconUrl", QUrl::fromLocalFile(entry->package.resolve(
                                 entry->package.manifest.value("icon").toString())));
    return result;
}
bool PluginRuntime::clearFailure(const QString &installation)
{
    auto *entry = find(installation);
    if (!entry || !entry->error.isEmpty())
        return false;
    entry->failure.clear();
    m_preferences.remove("plugins/failures/" + installation);
    changed(*entry);
    return true;
}
bool PluginRuntime::setSafeMode(bool enabled)
{
    if (m_closing || m_maintenance || loading())
        return false;
    if (m_safeMode == enabled)
        return true;
    m_safeMode = enabled;
    m_preferences.setValue("plugins/safeMode", enabled);
    m_preferences.sync();
    for (const auto &entry : m_entries) {
        if (enabled && entry->context)
            retire(*entry, Disabled);
        changed(*entry);
    }
    emit safeModeChanged();
    return true;
}
void PluginRuntime::refresh()
{
    if (m_closing || m_maintenance || loading())
        return;
    discoverRoot(true);
}
void PluginRuntime::discoverRoot(bool reloadInstalled)
{
    m_startScanPending = false;
    m_refreshing = true;
    ++m_imports;
    emit loadingChanged();
    auto *watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, reloadInstalled] {
        --m_imports;
        m_refreshSources = watcher->result();
        watcher->deleteLater();
        if (!reloadInstalled) {
            for (const auto &entry : m_entries)
                m_refreshSources.removeAll(
                    entry->origin.value("file", entry->origin.value("directory")).toString());
            const auto sources = std::exchange(m_refreshSources, {});
            m_refreshing = false;
            for (const auto &path : sources)
                importDirectory(path);
            queueReconcile();
        }
        if (reloadInstalled) {
            for (const auto &entry : m_entries) {
                // A root-folder update already reloads this installation through
                // the data snapshot/activation rollback transaction.
                if (!m_refreshSources.contains(
                        entry->origin.value("file", entry->origin.value("directory")).toString()))
                    m_refreshInstallations.append(entry->package.installationId);
            }
        }
        advanceRefresh();
        checkClosed();
    });
    watcher->setFuture(QtConcurrent::run([root = m_root] {
        QStringList sources;
        QDir directory(root);
        if (!directory.mkpath("."))
            return sources;
        for (const auto &name :
             directory.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
            const auto path = rootSource(directory.filePath(name), root);
            if (!path.isEmpty() &&
                (QFileInfo(path).isFile() || QFileInfo(path + "/manifest.json").isFile()))
                sources.append(path);
        }
        return sources;
    }));
}
void PluginRuntime::advanceRefresh()
{
    if (m_maintenance)
        return;
    if (m_closing) {
        m_startScanPending = m_refreshing = false;
        m_refreshSources.clear();
        m_refreshInstallations.clear();
        return;
    }
    if (m_imports)
        return;
    for (const auto &entry : m_entries)
        if (entry->state == Discovered || entry->state == Activating ||
            entry->state == Deactivating || entry->mutation != Entry::None)
            return;
    if (m_startScanPending) {
        discoverRoot(false);
        return;
    }
    if (!m_refreshing)
        return;
    while (!m_refreshSources.isEmpty()) {
        const auto path = m_refreshSources.takeFirst();
        if (installDirectory(path, {{"kind", "local"}, {"directory", path}}))
            return;
        emit managementFinished({}, false, tr("无法导入插件目录：%1").arg(path));
    }
    while (!m_refreshInstallations.isEmpty()) {
        if (reload(m_refreshInstallations.takeFirst()))
            return;
    }
    m_refreshing = false;
    queueReconcile();
    emit loadingChanged();
}
bool PluginRuntime::installDirectory(const QString &directory, const QVariantMap &origin,
                                     const QVariantMap &expected,
                                     QSharedPointer<QTemporaryDir> sourceLease)
{
    if (m_closing || m_maintenance || m_imports)
        return false;
    if (QFileInfo(directory).isFile()) {
        if (!directory.endsWith(".echomod", Qt::CaseInsensitive))
            return false;
        const auto stage = QSharedPointer<QTemporaryDir>::create();
        ++m_imports;
        emit loadingChanged();
        auto *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcherBase::finished, this,
                [this, watcher, stage, origin, expected] {
                    --m_imports;
                    queueReconcile();
                    const auto error = watcher->result();
                    watcher->deleteLater();
                    if (m_closing || !error.isEmpty() ||
                        !installDirectory(stage->path(), origin, expected, stage))
                        emit managementFinished(
                            {}, false, error.isEmpty() ? tr("无法安装 .echomod 插件包") : error);
                    emit loadingChanged();
                    checkClosed();
                });
        watcher->setFuture(QtConcurrent::run([directory, stage] {
            return stage->isValid() ? PluginPackage::extractArchive(directory, stage->path())
                                    : QStringLiteral("Cannot create plugin staging directory");
        }));
        return true;
    }
    const auto sourcePath = QFileInfo(directory).canonicalFilePath();
    const auto rootPath = QFileInfo(m_root).canonicalFilePath();
    if (!rootPath.isEmpty() && (sourcePath == rootPath || sourcePath.startsWith(rootPath + '/')) &&
        rootSource(directory, m_root).isEmpty())
        return false;
    auto installationOrigin = origin;
    if (const auto owned = rootSource(directory, m_root); !owned.isEmpty()) {
        installationOrigin = {{"kind", "local"}, {"directory", owned}};
    }
    QFile manifestFile(directory + "/manifest.json");
    if (!manifestFile.open(QIODevice::ReadOnly) || manifestFile.size() > 256 * 1024)
        return false;
    const auto manifestBytes = manifestFile.readAll();
    const auto descriptor = PluginPackage::inspectManifest(manifestBytes);
    if (!descriptor.error.isEmpty() || (!expected.isEmpty() && descriptor.manifest != expected))
        return false;
    const auto existing = installationForId(descriptor.id);
    auto *target = find(existing);
    const auto plan = dependencyPlan(&descriptor);
    if (target && plan.errors.contains(descriptor.id))
        return false;
    for (const auto &other : m_entries)
        if (other->context && plan.errors.contains(other->package.id))
            return false; // A provider update cannot silently break live consumers.
    if (target && (identityKey(descriptor.id, target->origin) !=
                   identityKey(descriptor.id, installationOrigin)))
        return false;
    if (target && (target->mutation != Entry::None || target->state == Discovered ||
                   target->state == Activating || target->state == Deactivating))
        return false;
    auto installation =
        existing.isEmpty()
            ? m_preferences.value(identityKey(descriptor.id, installationOrigin)).toString()
            : existing;
    if (installation.isEmpty())
        installation = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ++m_imports;
    emit managementStarted(installation, descriptor.id);
    emit loadingChanged();
    auto *watcher = new QFutureWatcher<PluginPackage>(this);
    connect(watcher, &QFutureWatcher<PluginPackage>::finished, this,
            [this, watcher, existing, installation, installationOrigin, expected] {
                --m_imports;
                queueReconcile();
                auto package = watcher->result();
                watcher->deleteLater();
                if (m_closing || !package.error.isEmpty() ||
                    (!expected.isEmpty() && package.manifest != expected)) {
                    emit managementFinished(installation, false,
                                            m_closing                 ? tr("应用正在关闭")
                                            : package.error.isEmpty() ? tr("安装过程中清单已变更")
                                                                      : package.error);
                    emit loadingChanged();
                    checkClosed();
                    return;
                }
                auto *entry = find(existing);
                if (!entry) {
                    beginInsertRows({}, rowCount(), rowCount());
                    auto created = std::make_unique<Entry>();
                    created->package.installationId = installation;
                    created->enabled = true;
                    created->newInstall = true;
                    entry = created.get();
                    m_entries.push_back(std::move(created));
                    endInsertRows();
                }
                entry->previousPackage = entry->package;
                entry->previousOrigin = entry->origin;
                entry->previousSource = entry->source;
                entry->pendingPackage = std::move(package);
                entry->origin = installationOrigin;
                entry->mutation = Entry::Updating;
                entry->applying = false;
                if (entry->context)
                    retire(*entry, Disabled);
                else
                    applyMutation(*entry);
            });
    watcher->setFuture(
        QtConcurrent::run([directory, packages = m_root + "/packages", installation, sourceLease] {
            // Keep downloaded input alive even if its manager is destroyed while copying.
            Q_UNUSED(sourceLease)
            return PluginPackage::importDirectory(directory, packages, installation);
        }));
    return true;
}
void PluginRuntime::applyMutation(Entry &entry)
{
    if (entry.mutation == Entry::Removing) {
        removeEntry(entry);
        return;
    }
    const bool snapshot =
        entry.mutation == Entry::Updating && !entry.newInstall && !entry.snapshotReady;
    const bool restore = entry.mutation == Entry::Rollback && !entry.dataRestored;
    if (snapshot || restore) {
        ++m_imports;
        changed(entry);
        const auto installation = entry.package.installationId;
        auto *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcherBase::finished, this,
                [this, watcher, target = &entry, snapshot, installation] {
                    --m_imports;
                    queueReconcile();
                    const auto error = watcher->result();
                    watcher->deleteLater();
                    if (m_closing) {
                        checkClosed();
                        return;
                    }
                    if (!error.isEmpty()) {
                        target->transactionError = error;
                        if (snapshot) {
                            target->snapshotReady = true;
                            target->dataRestored = true;
                            target->mutation = Entry::Rollback;
                            target->pendingPackage = target->previousPackage;
                            target->origin = target->previousOrigin;
                            applyMutation(*target);
                        } else {
                            target->package = target->previousPackage;
                            target->origin = target->previousOrigin;
                            target->source = target->previousSource;
                            target->state = Failed;
                            target->error = error;
                            target->applying = true;
                            finishMutation(*target);
                        }
                        return;
                    }
                    if (snapshot) {
                        target->snapshotReady = true;
                        m_preferences.setValue("plugins/rollback/" + installation, true);
                        m_preferences.sync();
                        if (m_preferences.status() != QSettings::NoError) {
                            target->transactionError = "Cannot persist update rollback journal";
                            target->mutation = Entry::Rollback;
                            target->pendingPackage = target->previousPackage;
                            target->origin = target->previousOrigin;
                        }
                    } else
                        target->dataRestored = true;
                    applyMutation(*target);
                });
        watcher->setFuture(QtConcurrent::run([root = m_root, installation, snapshot] {
            QDir().mkpath(root + "/transactions");
            const auto data = root + "/data/" + installation;
            const auto backup = root + "/transactions/" + installation;
            return snapshot ? PluginDataTransaction::capture(data, backup)
                            : PluginDataTransaction::restore(data, backup);
        }));
        return;
    }
    entry.applying = true;
    entry.package = entry.pendingPackage;
    entry.source = entry.package.root;
    entry.error.clear();
    entry.state = Disabled;
    changed(entry);
    if (entry.enabled && !m_safeMode)
        queueReconcile();
    else
        finishMutation(entry);
}
void PluginRuntime::finishMutation(Entry &entry)
{
    if (entry.mutation == Entry::None || !entry.applying ||
        (entry.context && entry.state != Active))
        return;
    if ((entry.state == Failed || entry.state == Blocked) && entry.mutation == Entry::Updating &&
        !entry.newInstall) {
        entry.transactionError = entry.error;
        entry.mutation = Entry::Rollback;
        entry.pendingPackage = entry.previousPackage;
        entry.origin = entry.previousOrigin;
        applyMutation(entry);
        return;
    }
    const auto mutation = entry.mutation;
    bool success = mutation == Entry::Updating && entry.state != Failed;
    auto error = entry.transactionError.isEmpty() ? entry.error : entry.transactionError;
    const auto installation = entry.package.installationId;
    const bool retainedSnapshot = entry.snapshotReady && (entry.dataRestored || success);
    // Publish catalog and removal of the rollback journal in one atomic sync.
    // Keep both versions and the data snapshot until that durable commit succeeds.
    entry.mutation = Entry::None;
    m_preferences.setValue(identityKey(entry.package.id, entry.origin), installation);
    if (retainedSnapshot)
        m_preferences.remove("plugins/rollback/" + installation);
    persist();
    if (m_preferences.status() != QSettings::NoError) {
        if (entry.snapshotReady)
            m_preferences.setValue("plugins/rollback/" + installation, true);
        error = "Cannot persist plugin installation metadata";
        // A rollback can also have an active restored context. If its metadata
        // cannot commit, retire that code before reporting transaction failure;
        // this is a real storage error, independent of process-exit detection.
        if (success || entry.context) {
            entry.mutation = mutation;
            entry.transactionError = error;
            if (entry.context)
                retire(entry, Failed, error);
            else {
                entry.state = Failed;
                entry.error = error;
                finishMutation(entry);
            }
            return;
        }
        success = false;
    }
    entry.applying = false;
    entry.newInstall = false;
    entry.previousPackage = {};
    entry.pendingPackage = {};
    entry.previousOrigin.clear();
    entry.previousSource.clear();
    entry.transactionError.clear();
    entry.snapshotReady = false;
    entry.dataRestored = false;
    changed(entry);
    auto notify = [this, installation, success, error] {
        emit managementFinished(installation, success, error);
    };
    if (retainedSnapshot && m_preferences.status() == QSettings::NoError)
        discardSnapshot(m_root + "/transactions/" + installation, notify);
    else
        notify();
}

bool PluginRuntime::uninstall(const QString &installation, bool removeData)
{
    auto *entry = find(installation);
    if (!entry || m_closing || m_maintenance || m_imports || entry->mutation != Entry::None ||
        entry->state == Activating || entry->state == Deactivating || entry->state == Discovered)
        return false;
    entry->mutation = Entry::Removing;
    entry->removeData = removeData;
    entry->applying = false;
    if (entry->context)
        retire(*entry, Disabled);
    else
        removeEntry(*entry);
    return true;
}
void PluginRuntime::removeEntry(Entry &entry)
{
    const auto installation = entry.package.installationId;
    // Atomically detach the catalog after all JS leases and SQL jobs have drained.
    // Package reclamation can fail independently without restoring executable state.
    int row = 0;
    while (m_entries[size_t(row)].get() != &entry)
        ++row;
    const bool removeData = entry.removeData;
    const auto key = identityKey(entry.package.id, entry.origin);
    const auto input =
        entry.origin.value("kind") == "local"
            ? rootSource(entry.origin.value("file", entry.origin.value("directory")).toString(),
                         m_root)
            : QString();
    beginRemoveRows({}, row, row);
    m_entries.erase(m_entries.begin() + row);
    endRemoveRows();
    if (removeData)
        m_preferences.remove(key);
    m_preferences.remove("plugins/failures/" + installation);
    persist();
    emit catalogChanged();
    ++m_imports;
    emit loadingChanged();
    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, installation] {
        --m_imports;
        queueReconcile();
        const bool ok = watcher->result();
        watcher->deleteLater();
        emit managementFinished(installation, ok,
                                ok ? QString() : tr("插件已卸载，但部分文件无法清理"));
        emit loadingChanged();
        checkClosed();
    });
    watcher->setFuture(QtConcurrent::run([root = m_root, installation, removeData, input] {
        const bool source =
            input.isEmpty() || !QFileInfo::exists(input) ||
            (rootSource(input, root) == input &&
             (QFileInfo(input).isFile() ? QFile::remove(input) : QDir(input).removeRecursively()));
        const bool packages = QDir(root + "/packages/" + installation).removeRecursively();
        const bool data = !removeData || QDir(root + "/data/" + installation).removeRecursively();
        return source && packages && data;
    }));
}
void PluginRuntime::discardSnapshot(const QString &backup, std::function<void()> complete)
{
    ++m_imports;
    emit loadingChanged();
    auto *watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, complete] {
        --m_imports;
        // A provider may have committed while snapshot cleanup still held the
        // import barrier. Resume its waiting consumers when that barrier drops.
        queueReconcile();
        watcher->deleteLater();
        emit loadingChanged();
        if (complete)
            complete();
        checkClosed();
    });
    watcher->setFuture(QtConcurrent::run([backup] { PluginDataTransaction::discard(backup); }));
}
void PluginRuntime::shutdown()
{
    if (std::exchange(m_closing, true))
        return;
    if (auto ready = std::exchange(m_maintenanceReady, {}))
        ready(false);
    for (const auto &entry : m_entries)
        if (entry->context)
            retire(*entry, Disabled);
    checkClosed();
}
bool PluginRuntime::beginMaintenance(std::function<void(bool)> ready)
{
    if (m_closing || m_maintenance || loading())
        return false;
    for (const auto &entry : m_entries)
        if (entry->mutation != Entry::None)
            return false;
    m_maintenance = true;
    emit loadingChanged();
    m_maintenanceReady = std::move(ready);
    for (const auto &entry : m_entries)
        if (entry->context)
            retire(*entry, Disabled);
    QTimer::singleShot(0, this, &PluginRuntime::checkMaintenance);
    return true;
}
void PluginRuntime::checkMaintenance()
{
    if (!m_maintenance || !m_maintenanceReady || m_closing)
        return;
    for (const auto &entry : m_entries)
        if (entry->context)
            return;
    auto ready = std::exchange(m_maintenanceReady, {});
    ready(true);
}
void PluginRuntime::endMaintenance()
{
    if (!m_maintenance)
        return;
    m_maintenance = false;
    auto ready = std::exchange(m_maintenanceReady, {});
    if (ready)
        ready(false);
    queueReconcile();
    emit loadingChanged();
}
QVariantList PluginRuntime::backupEntries() const
{
    Q_ASSERT(m_maintenance || m_closed);
    QVariantList entries;
    for (const auto &entry : m_entries) {
        if (!entry->package.error.isEmpty() || entry->package.root.isEmpty())
            continue;
        auto origin = entry->origin;
        origin.remove("directory");
        entries.append(QVariantMap{{"id", entry->package.id},
                                   {"name", entry->package.name},
                                   {"installation", entry->package.installationId},
                                   {"root", entry->package.root},
                                   {"origin", origin},
                                   {"enabled", entry->enabled}});
    }
    return entries;
}
void PluginRuntime::checkClosed()
{
    if (!m_closing || m_closed || m_imports)
        return;
    for (const auto &entry : m_entries)
        if (entry->context)
            return;
    m_closed = true;
    // One queued boundary releases pending page delegates/context deleteLater.
    QTimer::singleShot(0, this, [this] { emit closed(); });
}
} // namespace EchoMusic
