#pragma once
#include "StartupController.h"
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
struct StartupControllerRegistration
{
    Q_GADGET
    QML_FOREIGN(StartupController)
    QML_NAMED_ELEMENT(StartupController)
    QML_UNCREATABLE("Application-owned initialization lifecycle")
};
} // namespace EchoMusic
