#include "HyprlandWindowBackdrop.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QWindow>
namespace EchoMusic
{
HyprlandWindowBackdrop::HyprlandWindowBackdrop(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(2000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_process) {
            m_failure = tr("窗口毛玻璃设置超时");
            m_process->kill();
        }
    });
}
HyprlandWindowBackdrop::~HyprlandWindowBackdrop()
{
    stop();
}
bool HyprlandWindowBackdrop::available()
{
#ifdef Q_OS_LINUX
    const bool compositor =
        !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE") ||
        qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("Hyprland", Qt::CaseInsensitive) ||
        qEnvironmentVariable("XDG_SESSION_DESKTOP").contains("Hyprland", Qt::CaseInsensitive);
    return compositor && !QStandardPaths::findExecutable("hyprctl").isEmpty();
#else
    return false;
#endif
}
void HyprlandWindowBackdrop::stop()
{
    m_timeout.stop();
    if (m_process) {
        auto *process = m_process.data();
        disconnect(process, nullptr, this, nullptr);
        if (process->state() != QProcess::NotRunning) {
            connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
                    &QObject::deleteLater);
            process->kill();
        } else
            process->deleteLater();
        m_process = nullptr;
    }
    m_busy = false;
}
void HyprlandWindowBackdrop::attach(QWindow *window)
{
    if (window && m_window == window)
        return;
    stop();
    m_window = window;
    m_applied.reset();
}
QString HyprlandWindowBackdrop::windowAddress(const QByteArray &clients, qint64 pid,
                                              const QString &title)
{
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(clients, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return {};
    const QRegularExpression address("^0x[0-9a-fA-F]+$");
    QString unique;
    int candidates = 0;
    for (const auto &entry : document.array()) {
        const auto client = entry.toObject();
        const auto value = client.value("address").toString();
        if (client.value("pid").toInteger(-1) != pid || !address.match(value).hasMatch())
            continue;
        if (client.value("title").toString() == title)
            return value;
        unique = value;
        ++candidates;
    }
    // Avoid changing an arbitrary secondary/plugin window sharing our PID.
    return candidates == 1 ? unique : QString();
}
void HyprlandWindowBackdrop::apply(bool enabled)
{
    m_desired = enabled;
    if (!m_busy && (!m_applied || *m_applied != enabled))
        start();
}
void HyprlandWindowBackdrop::start()
{
    if (!m_window)
        return;
    m_busy = true;
    m_runningValue = m_desired;
    m_address.clear();
    run({"clients", "-j"}, 0);
}
void HyprlandWindowBackdrop::run(const QStringList &arguments, int step)
{
    if (!m_window) {
        finish({});
        return;
    }
    auto *process = new QProcess(this);
    m_process = process;
    m_output.clear();
    m_failure.clear();
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        m_output.append(process->readAllStandardOutput());
        if (m_output.size() > 1024 * 1024) {
            m_failure = tr("合成器响应超过大小限制");
            process->kill();
        }
    });
    // Parse compositor JSON only from stdout.
    // Drain stderr separately so diagnostics cannot corrupt clients -j.
    connect(process, &QProcess::readyReadStandardError, this,
            [process] { process->readAllStandardError(); });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_process = nullptr;
            process->deleteLater();
            finish(tr("无法启动窗口合成器控制工具"));
        }
    });
    connect(
        process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, step](int code, QProcess::ExitStatus status) {
            m_timeout.stop();
            m_output.append(process->readAllStandardOutput());
            m_process = nullptr;
            process->deleteLater();
            if (!m_window) {
                finish({});
                return;
            }
            const bool ok = code == 0 && status == QProcess::NormalExit && m_failure.isEmpty();
            if (step == 0 && ok) {
                m_address =
                    windowAddress(m_output, QCoreApplication::applicationPid(), m_window->title());
                if (m_address.isEmpty()) {
                    finish(tr("未找到当前窗口的合成器地址"));
                    return;
                }
                const auto noBlur = m_runningValue ? "0" : "1";
                run({"dispatch", QString("hl.dsp.window.set_prop({ prop = \"no_blur\", value = "
                                         "\"%1\", window = \"address:%2\" })")
                                     .arg(noBlur, m_address)},
                    1);
            } else if (step == 1 && !ok) {
                run({"setprop", "address:" + m_address, "noblur", m_runningValue ? "0" : "1"}, 2);
            } else
                finish(ok                    ? QString()
                       : m_failure.isEmpty() ? tr("窗口合成器未能应用毛玻璃设置")
                                             : m_failure);
        });
    process->start(QStandardPaths::findExecutable("hyprctl"), arguments);
    m_timeout.start();
}
void HyprlandWindowBackdrop::finish(const QString &error)
{
    m_timeout.stop();
    m_busy = false;
    if (error.isEmpty() && m_window)
        m_applied = m_runningValue;
    emit completed(error);
    if (m_window && m_desired != m_runningValue)
        start();
}
} // namespace EchoMusic
