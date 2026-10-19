#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <optional>
class QProcess;
class QWindow;
namespace EchoMusic
{
/// Per-window asynchronous compositor policy. No shell or global config edits.
class HyprlandWindowBackdrop final : public QObject
{
    Q_OBJECT
public:
    explicit HyprlandWindowBackdrop(QObject *parent = nullptr);
    ~HyprlandWindowBackdrop() override;
    static bool available();
    void attach(QWindow *window);
    void apply(bool enabled);
    /// Resolve compositor addresses by process/title; Wayland WId is not an address.
    static QString windowAddress(const QByteArray &clients, qint64 pid, const QString &title);
signals:
    void completed(const QString &error);

private:
    void start();
    void run(const QStringList &arguments, int step);
    void finish(const QString &error);
    void stop();
    QPointer<QWindow> m_window;
    QPointer<QProcess> m_process;
    QTimer m_timeout;
    QByteArray m_output;
    QString m_address, m_failure;
    bool m_desired = false, m_runningValue = false, m_busy = false;
    std::optional<bool> m_applied;
};
} // namespace EchoMusic
