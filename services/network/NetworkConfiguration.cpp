#include "NetworkConfiguration.h"
#include <QRegularExpression>
#include <QObject>
#include <cmath>

namespace EchoMusic
{
QString NetworkConfiguration::modeKey(Mode mode)
{
    switch (mode) {
    case Mode::Direct:
        return QStringLiteral("direct");
    case Mode::System:
        return QStringLiteral("system");
    case Mode::AutoDetect:
        return QStringLiteral("auto_detect");
    case Mode::PacScript:
        return QStringLiteral("pac_script");
    case Mode::FixedServers:
        return QStringLiteral("fixed_servers");
    }
    return QStringLiteral("direct");
}
NetworkConfiguration::Mode NetworkConfiguration::modeFromKey(const QString &key)
{
    for (const auto mode : {Mode::System, Mode::AutoDetect, Mode::PacScript, Mode::FixedServers})
        if (modeKey(mode) == key)
            return mode;
    return Mode::Direct;
}
QString NetworkConfiguration::normalizedPacScript(const QString &value)
{
    QUrl url(value.trimmed(), QUrl::TolerantMode);
    const auto scheme = url.scheme().toLower();
    if (!url.isValid() || !QStringList{"http", "https", "file"}.contains(scheme) ||
        !url.userName().isEmpty() || !url.password().isEmpty() ||
        (scheme != "file" && url.host().isEmpty()))
        return {};
    if (url.path().isEmpty())
        url.setPath("/");
    return url.toString(QUrl::FullyEncoded);
}
std::optional<QList<NetworkProxyGroup>> NetworkConfiguration::parseProxyRules(const QString &value)
{
    const auto rules = value.trimmed();
    if (rules.isEmpty() || rules.size() > 4096)
        return std::nullopt;
    for (const auto c : rules)
        if (c.unicode() < 32 || c.unicode() == 127)
            return std::nullopt;
    QList<NetworkProxyGroup> groups;
    for (const auto &raw : rules.split(';')) {
        NetworkProxyGroup group;
        const int separator = raw.indexOf('=');
        if (separator >= 0) {
            group.protocol = raw.left(separator).trimmed();
            if (!QStringList{"http", "https", "ftp", "socks"}.contains(group.protocol.toLower()))
                return std::nullopt;
        }
        for (const auto &rawEndpoint : raw.mid(separator + 1).split(',')) {
            const auto text = rawEndpoint.trimmed();
            if (text.isEmpty())
                return std::nullopt;
            for (const auto c : text)
                if (c.isSpace())
                    return std::nullopt;
            if (text.compare("direct://", Qt::CaseInsensitive) == 0) {
                group.endpoints.append(NetworkProxyEndpoint{});
                continue;
            }
            // Chromium's manual URI "socks://" means SOCKS5; a bare endpoint
            // inside the socks= fallback group instead defaults to SOCKS4.
            const QUrl url(text.contains("://")        ? text
                           : group.protocol == "socks" ? "socks4://" + text
                                                       : "http://" + text,
                           QUrl::StrictMode);
            if (!url.isValid() || url.host().isEmpty() || !url.userName().isEmpty() ||
                !url.password().isEmpty() || !url.query().isEmpty() || !url.fragment().isEmpty() ||
                (!url.path().isEmpty() && url.path() != "/"))
                return std::nullopt;
            NetworkProxyEndpoint::Kind kind;
            const auto scheme = url.scheme().toLower();
            if (scheme == "http")
                kind = NetworkProxyEndpoint::Kind::Http;
            else if (scheme == "https")
                kind = NetworkProxyEndpoint::Kind::Https;
            else if (scheme == "socks4")
                kind = NetworkProxyEndpoint::Kind::Socks4;
            else if (scheme == "socks" || scheme == "socks5")
                kind = NetworkProxyEndpoint::Kind::Socks5;
            else
                return std::nullopt;
            group.endpoints.append({kind, url});
        }
        groups.append(group);
    }
    return groups;
}
QString NetworkConfiguration::normalizedBypassRules(const QString &value)
{
    QStringList result;
    for (const auto &entry : value.split(QRegularExpression("[;,]"))) {
        const auto rule = entry.trimmed();
        if (!rule.isEmpty())
            result.append(rule);
        if (result.size() == 100)
            break;
    }
    return result.isEmpty() ? QStringLiteral("<local>") : result.join(',');
}
NetworkConfiguration NetworkConfiguration::normalized() const
{
    auto result = *this;
    result.pacScript = normalizedPacScript(pacScript);
    result.proxyRules = parseProxyRules(proxyRules) ? proxyRules.trimmed() : QString();
    result.username = username.trimmed();
    result.bypassRules = normalizedBypassRules(bypassRules);
    result.apiTimeoutSeconds =
        std::isfinite(apiTimeoutSeconds) ? qBound(0., apiTimeoutSeconds, 300.) : 0.;
    result.playerTimeoutSeconds =
        std::isfinite(playerTimeoutSeconds) ? qBound(1., playerTimeoutSeconds, 300.) : 60.;
    return result;
}
QString NetworkConfiguration::validationError() const
{
    if (mode == Mode::PacScript && normalizedPacScript(pacScript).isEmpty())
        return QObject::tr("PAC 脚本地址无效");
    if (mode == Mode::FixedServers && !parseProxyRules(proxyRules))
        return QObject::tr("手动代理规则无效");
    return {};
}
} // namespace EchoMusic
