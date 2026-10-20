#pragma once
#include <QString>

class QSettings;
namespace EchoMusic
{

/// An immutable request snapshot. Account credentials belong to the account
/// service; device preferences contain no login token and use the new app namespace.
struct DeviceIdentity
{
    QString mid;
    QString dfid;
    QString guid;
    QString mac;
    // Fixed application name in registration/login proofs, like SERVER_DEV in
    // EchoMusic; GUID/MID remain independent persistent device identifiers.
    QString deviceName;
    QString uuid;
    QString renderFingerprint;
    static DeviceIdentity load(QSettings &settings);
    static DeviceIdentity temporary();
    static bool saveRegistration(QSettings &settings, const DeviceIdentity &device);
    /// Clear local identity and fence late registration writes until the next launch.
    static bool resetOnRestart(QSettings &settings);
    static QString midForGuid(const QString &guid);
};
} // namespace EchoMusic
