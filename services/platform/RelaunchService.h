#pragma once
#include <QObject>
#include <QLocalServer>
#include <QPointer>
#include <QTimer>
#include <QStringList>

class QLocalSocket;
namespace EchoMusic
{
/// Prepares a Core-only helper before requesting the normal asynchronous service
/// shutdown. The helper waits for the single-instance lock, including SQL/audio/
/// logging teardown, before launching the same executable. No PID killing/shell.
class RelaunchService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool automaticRestart READ automaticRestart CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit RelaunchService(QString lockPath, QStringList arguments, bool automaticRestart,
                             QObject *parent = nullptr);
    ~RelaunchService() override;
    bool automaticRestart() const
    {
        return m_automatic;
    }
    bool busy() const
    {
        return m_busy;
    }
    Q_INVOKABLE void restart();
    void restartForStartupRecovery();
    static bool isHelperInvocation(int argc, char **argv);
    static int runHelper(int argc, char **argv);
    static QStringList persistentArguments(const QStringList &arguments);
signals:
    void changed();
    void notice(const QString &message);
    void readyToQuit();

private:
    void prepareRestart(bool allowed);
    void fail();
    QLocalServer m_server;
    QPointer<QLocalSocket> m_socket;
    QTimer m_deadline;
    QString m_lockPath;
    QStringList m_arguments;
    QByteArray m_response;
    bool m_automatic = false, m_busy = false, m_prepared = false, m_committed = false;
};
} // namespace EchoMusic
