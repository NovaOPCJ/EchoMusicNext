#pragma once
#include <QString>

namespace EchoMusic
{
// One process identity shared by the pre-Qt crash collector and the log sink.
const QString &sessionLogFileName();
} // namespace EchoMusic
