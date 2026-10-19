#pragma once
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <optional>

namespace EchoMusic
{
/// Immutable configuration copied to transport workers on application commit.
/// Passwords never enter this value, settings files, QML properties or logs.
struct NetworkProxyEndpoint
{
    enum class Kind
    {
        Direct,
        Http,
        Https,
        Socks4,
        Socks5
    };
    Kind kind = Kind::Direct;
    QUrl address;
    bool operator==(const NetworkProxyEndpoint &) const = default;
};
struct NetworkProxyGroup
{
    QString protocol;
    QList<NetworkProxyEndpoint> endpoints;
    bool operator==(const NetworkProxyGroup &) const = default;
};
struct NetworkConfiguration
{
    enum class Mode
    {
        Direct,
        System,
        AutoDetect,
        PacScript,
        FixedServers
    };
    Mode mode = Mode::Direct;
    QString pacScript, proxyRules, username;
    QString bypassRules = QStringLiteral("<local>");
    double apiTimeoutSeconds = 0, playerTimeoutSeconds = 60;
    bool operator==(const NetworkConfiguration &) const = default;

    static QString modeKey(Mode mode);
    static Mode modeFromKey(const QString &key);
    static QString normalizedPacScript(const QString &value);
    static std::optional<QList<NetworkProxyGroup>> parseProxyRules(const QString &value);
    static QString normalizedBypassRules(const QString &value);
    NetworkConfiguration normalized() const;
    QString validationError() const;
};
} // namespace EchoMusic
