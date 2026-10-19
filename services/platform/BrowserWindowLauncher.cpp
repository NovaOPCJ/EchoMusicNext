#include <QtCore/qglobal.h>
#if defined(Q_OS_LINUX)
#include <gio/gio.h>
#endif
#include "BrowserWindowLauncher.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <iterator>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlwapi.h>
#endif

namespace EchoMusic
{
#ifdef Q_OS_MACOS
QString defaultBrowserExecutable();
#else
namespace
{
QString defaultBrowserExecutable()
{
#ifdef Q_OS_WIN
    wchar_t path[32768];
    DWORD length = DWORD(std::size(path));
    if (SUCCEEDED(AssocQueryStringW(ASSOCF_IS_PROTOCOL, ASSOCSTR_EXECUTABLE, L"http", L"open", path,
                                    &length)))
        return QString::fromWCharArray(path);
#elif defined(Q_OS_LINUX)
    GAppInfo *app = g_app_info_get_default_for_uri_scheme("http");
    if (app) {
        const auto executable = QString::fromLocal8Bit(g_app_info_get_executable(app));
        const auto command =
            QProcess::splitCommand(QString::fromLocal8Bit(g_app_info_get_commandline(app)));
        g_object_unref(app);
        // Preserve custom desktop-entry/profile arguments through the OS opener.
        // Only plain browser handlers may use our app-window launch arguments.
        if (command.size() != 2 || command[0] != executable ||
            (command[1] != "%u" && command[1] != "%U"))
            return {};
        return QStandardPaths::findExecutable(executable);
    }
#endif
    return {};
}
} // namespace
#endif

QStringList BrowserWindowLauncher::windowArguments(const QString &executable, const QUrl &url)
{
    const auto name = QFileInfo(executable).fileName().toLower();
    // Do not forward Chromium switches to arbitrary URI handlers or wrappers.
    static const QStringList browsers{
        "google chrome",    "google chrome canary", "chromium",
        "microsoft edge",   "microsoft edge beta",  "brave browser",
        "chrome.exe",       "msedge.exe",           "brave.exe",
        "chromium.exe",     "google-chrome",        "google-chrome-stable",
        "chromium-browser", "microsoft-edge",       "microsoft-edge-stable",
        "brave-browser"};
    if (!browsers.contains(name) || url.scheme() != "http" || url.host() != "127.0.0.1" ||
        url.path() != "/verification" || url.port() < 1)
        return {};
    return {QStringLiteral("--app=") + url.toString(QUrl::FullyEncoded),
            QStringLiteral("--window-size=440,680")};
}
bool BrowserWindowLauncher::openVerification(const QUrl &url)
{
    const auto executable = defaultBrowserExecutable();
    const auto arguments = windowArguments(executable, url);
    if (!arguments.isEmpty()) {
        QProcess browser;
        browser.setProgram(executable);
        browser.setArguments(arguments);
        browser.setStandardOutputFile(QProcess::nullDevice());
        browser.setStandardErrorFile(QProcess::nullDevice());
        if (browser.startDetached())
            return true;
    }
    return QDesktopServices::openUrl(url);
}
} // namespace EchoMusic
