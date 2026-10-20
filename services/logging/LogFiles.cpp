#include "LogFiles.h"
#include <QCoreApplication>
#include <QDateTime>

namespace EchoMusic
{
const QString &sessionLogFileName()
{
    static const auto name =
        QStringLiteral("echomusicnext-%1-%2.log")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss-zzz"))
            .arg(QCoreApplication::applicationPid());
    return name;
}
} // namespace EchoMusic
