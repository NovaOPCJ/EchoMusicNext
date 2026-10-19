#pragma once
#include <QTemporaryDir>
#include <QSharedPointer>
#include "RelaunchService.h"
#include "SettingsBackupArchive.h"
#include "NetworkPreferences.h"
#include <QFutureWatcher>
#include <QThreadPool>
#include <QTimer>
#include <limits>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
class PluginRuntime;
/// Settings backup UI owns only choices. This service owns the private inspected
/// input, bounded worker, plugin drain barrier and post-shutdown restore commit.
class SettingsDataService final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Application-owned backup service")
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
    Q_PROPERTY(bool canRestore READ canRestore NOTIFY changed)
public:
    SettingsDataService(QSettings &settings, PluginRuntime &plugins, RelaunchService &relaunch,
                        NetworkPreferences &network, QString root, QString version,
                        QObject *parent = nullptr);
    ~SettingsDataService() override;
    bool busy() const
    {
        return m_busy;
    }
    QVariantMap summary() const;
    bool canRestore() const
    {
        return m_staging && !m_manifest.isEmpty() && !m_busy;
    }
    Q_INVOKABLE void create(const QUrl &file, bool settings, bool plugins);
    Q_INVOKABLE void inspect(const QUrl &file);
    Q_INVOKABLE void releaseInspection();
    Q_INVOKABLE void restore(bool settings, bool plugins);
    Q_INVOKABLE void clearApplicationData();
    Q_INVOKABLE void cancel();
    void shutdown();
    void setInstanceLockFilePath(const QString &path)
    {
        m_instanceLock = path;
    }
    void setMaintenanceGuard(std::function<bool()> guard)
    {
        m_guard = std::move(guard);
    }
    /// Called after all five application storage/audio/API services have closed;
    /// the relaunch helper still waits for the original single-instance lock.
    void finishShutdown(std::function<void(QString)> complete);
    static QString recover(const QString &root, QSettings &settings);
signals:
    void changed();
    void notice(const QString &message, bool failure);
    void created();
    void inspected();

private:
    QVariantMap preferenceValues() const;
    void finishJob(const QString &error);
    void clearFiles(std::function<void(QString)> complete);
    QSettings &m_settings;
    PluginRuntime &m_plugins;
    RelaunchService &m_relaunch;
    NetworkPreferences &m_network;
    QString m_root, m_version, m_instanceLock;
    QThreadPool m_workers;
    QFutureWatcher<SettingsBackupResult> m_work;
    std::shared_ptr<std::atomic_bool> m_cancel = std::make_shared<std::atomic_bool>(false);
    QSharedPointer<QTemporaryDir> m_staging;
    QJsonObject m_manifest;
    QTimer m_expiry, m_barrierDeadline;
    bool m_busy = false, m_closing = false, m_restore = false, m_clear = false;
    bool m_restoreSettings = false, m_restorePlugins = false;
    std::function<bool()> m_guard;
};
} // namespace EchoMusic
