#pragma once
#include "SharingCommands.h"
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
struct SharingCommandsRegistration
{
    Q_GADGET
    QML_FOREIGN(SharingCommands)
    QML_NAMED_ELEMENT(SharingCommands)
    QML_UNCREATABLE("Application-owned share publisher")
};
} // namespace EchoMusic
