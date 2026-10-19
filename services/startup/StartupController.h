#pragma once
#include <QObject>
#include <QTimer>
#include <QString>
namespace EchoMusic
{
class StorageService;
class SavedAccountModel;
class ApiClient;
class DeviceRegistrationService;
class PlaybackService;
class NetworkPreferences;
/// Coordinates existing authorities; never owns another account, device or audio core.
class StartupController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Stage stage READ stage NOTIFY changed)
    Q_PROPERTY(bool entered READ entered NOTIFY changed)
    Q_PROPERTY(bool failed READ failed NOTIFY changed)
    Q_PROPERTY(bool closing READ closing NOTIFY changed)
    Q_PROPERTY(bool canForceEnter READ canForceEnter NOTIFY changed)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY changed)
    Q_PROPERTY(QString forceEnterHint READ forceEnterHint NOTIFY changed)
public:
    enum class Stage
    {
        Idle,
        Storage,
        Runtime,
        Device,
        Account,
        Playback,
        Finishing,
        Failed,
        Entered,
        Closing
    };
    Q_ENUM(Stage)
    StartupController(StorageService &, ApiClient &, DeviceRegistrationService &, PlaybackService &,
                      NetworkPreferences &, SavedAccountModel &, QString databasePath,
                      QObject *parent = nullptr);
    explicit StartupController(QString recoveryError, QObject *parent = nullptr);
    void reportRestartFailure(const QString &message);
    ~StartupController() override;
    Stage stage() const
    {
        return m_stage;
    }
    bool entered() const
    {
        return m_entered;
    }
    bool failed() const
    {
        return m_stage == Stage::Failed;
    }
    bool closing() const
    {
        return m_stage == Stage::Closing;
    }
    bool canForceEnter() const
    {
        return failed() && m_allowContinue;
    }
    QString statusMessage() const
    {
        return m_message;
    }
    QString forceEnterHint() const
    {
        return m_hint;
    }
    Q_INVOKABLE void start();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void forceEnter();
    Q_INVOKABLE void requestQuit();
    void shutdown();
signals:
    void changed();
    void enteredMain();
    void quitRequested();
    void restartApplicationRequested();

private:
    explicit StartupController(QObject *parent);
    void setStage(Stage, const QString &);
    void fail(const QString &, const QString &hint = {});
    void beginStorage();
    void beginRuntime();
    void beginDevice();
    void observeDevice();
    void beginAccounts();
    void observeAccounts();
    void beginPlayback();
    void observePlayback();
    void enter();
    void cancelRuntimeRequest();
    StorageService *m_storage = nullptr;
    SavedAccountModel *m_accounts = nullptr;
    ApiClient *m_api = nullptr;
    DeviceRegistrationService *m_device = nullptr;
    PlaybackService *m_playback = nullptr;
    NetworkPreferences *m_network = nullptr;
    QString m_databasePath, m_message, m_hint, m_recoveryError;
    Stage m_stage = Stage::Idle, m_failedStage = Stage::Idle;
    QTimer m_deadline, m_finish;
    quint64 m_request = 0, m_attempt = 0;
    bool m_entered = false, m_allowContinue = false, m_retryDevice = false, m_retryEngine = false;
};
} // namespace EchoMusic
