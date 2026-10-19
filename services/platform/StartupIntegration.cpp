#include "StartupIntegration.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
namespace EchoMusic
{
#ifdef Q_OS_MACOS
bool macLoginItemEnabled();
QString setMacLoginItemEnabled(bool enabled);
#endif
namespace
{
#ifdef Q_OS_LINUX
QString linuxEntryPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
           "/autostart/EchoMusicNext.desktop";
}
#endif
} // namespace
QString StartupIntegration::desktopEntry(const QString &executable)
{
    if (executable.isEmpty() || executable.contains('=') || executable.contains(QChar::Null))
        return {};
    // Desktop Entry applies string-value unescaping BEFORE Exec argument
    // unquoting. Two layers are required: a path backslash needs four in the
    // file; a quoted dollar sign needs two. Never pass this through a shell.
    auto escaped = executable;
    escaped.replace("\\", "\\\\")
        .replace("\"", "\\\"")
        .replace("`", "\\`")
        .replace("$", "\\$")
        .replace("%", "%%");
    escaped.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t");
    return "[Desktop Entry]\nType=Application\nName=EchoMusicNext\nExec=\"" + escaped +
           "\"\nTerminal=false\nX-GNOME-Autostart-enabled=true\n";
}
bool StartupIntegration::enabled() const
{
    if (!m_native)
        return false;
#ifdef Q_OS_MACOS
    return macLoginItemEnabled();
#elif defined(Q_OS_WIN)
    QSettings startup("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      QSettings::NativeFormat);
    return startup.value("EchoMusicNext").toString() ==
           "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\"";
#else
    QFile file(linuxEntryPath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return file.readAll() == desktopEntry(QCoreApplication::applicationFilePath()).toUtf8();
#endif
}
QString StartupIntegration::setEnabled(bool enabled)
{
    if (!m_native)
        return QCoreApplication::translate("StartupIntegration",
                                           "当前运行环境不提供系统自启动注册");
#ifdef Q_OS_MACOS
    return setMacLoginItemEnabled(enabled);
#elif defined(Q_OS_WIN)
    QSettings startup("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      QSettings::NativeFormat);
    if (enabled)
        startup.setValue("EchoMusicNext",
                         "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) +
                             "\"");
    else
        startup.remove("EchoMusicNext");
    startup.sync();
    return startup.status() == QSettings::NoError
               ? QString()
               : QCoreApplication::translate("StartupIntegration", "系统自启动设置失败");
#else
    const auto path = linuxEntryPath();
    if (!enabled) {
        if (!QFile::exists(path) || QFile::remove(path))
            return {};
    } else if (QDir().mkpath(QFileInfo(path).absolutePath())) {
        QSaveFile file(path);
        const auto bytes = desktopEntry(QCoreApplication::applicationFilePath()).toUtf8();
        if (bytes.isEmpty())
            return QCoreApplication::translate("StartupIntegration",
                                               "应用路径无法注册为系统自启动项");
        if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit())
            return {};
    }
    return QCoreApplication::translate("StartupIntegration", "系统自启动设置失败");
#endif
}
} // namespace EchoMusic
