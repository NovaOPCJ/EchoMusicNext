#pragma once
#include <QString>
#include <QStringList>
#include <QUrl>

namespace EchoMusic
{
/// Uses the user's default browser/profile. Only recognized Chromium browsers
/// have a supported app-window command; other handlers use the OS URL opener.
class BrowserWindowLauncher final
{
public:
    static bool openVerification(const QUrl &url);
    static QStringList windowArguments(const QString &executable, const QUrl &url);
};
} // namespace EchoMusic
