#include "ProxyRouting.h"
#include <QHostAddress>
#include <QRegularExpression>

namespace EchoMusic
{
namespace
{
bool matches(const QUrl &url, QString rule)
{
    const auto host = url.host().toLower();
    QHostAddress ip;
    const bool literal = ip.setAddress(host);
    if (rule == "<local>")
        return !literal && !host.contains('.');
    const auto cidr = QHostAddress::parseSubnet(rule);
    if (cidr.second >= 0)
        return literal && ip.isInSubnet(cidr);
    QString scheme;
    const auto separator = rule.indexOf("://");
    if (separator >= 0) {
        scheme = rule.left(separator);
        rule.remove(0, separator + 3);
        if (scheme != url.scheme().toLower())
            return false;
    }
    int port = -1;
    const auto colon = rule.lastIndexOf(':');
    if (colon >= 0 && (!rule.contains(']') || colon > rule.indexOf(']')) &&
        (rule.count(':') == 1 || rule.startsWith('['))) {
        bool valid = false;
        port = rule.mid(colon + 1).toInt(&valid);
        if (!valid || port < 1 || port > 65535)
            return false;
        rule = rule.left(colon);
    }
    if (port >= 0 && url.port(url.scheme() == "https" || url.scheme() == "wss" ? 443 : 80) != port)
        return false;
    if (rule.startsWith('[') && rule.endsWith(']'))
        rule = rule.mid(1, rule.size() - 2);
    QHostAddress ruleIp;
    if (ruleIp.setAddress(rule))
        return literal && ip == ruleIp;
    if (rule.startsWith('.'))
        rule.prepend('*');
    // Chromium's '*' is a hostname glob; '?' is a literal, not a shell wildcard.
    auto expression = QRegularExpression::escape(rule);
    expression.replace("\\*", ".*");
    return QRegularExpression("\\A" + expression + "\\z").match(host).hasMatch();
}
} // namespace
bool ProxyRouting::implicitBypass(const QUrl &url)
{
    auto host = url.host().toLower();
    if (host.endsWith('.'))
        host.chop(1);
    if (host == "localhost" || host.endsWith(".localhost"))
        return true;
    QHostAddress ip(host);
    return ip.isLoopback() || ip.isInSubnet(QHostAddress("169.254.0.0"), 16) ||
           ip.isInSubnet(QHostAddress("fe80::"), 10);
}
bool ProxyRouting::bypasses(const QUrl &url, const QString &rules)
{
    bool result = implicitBypass(url);
    for (const auto &entry : rules.split(QRegularExpression("[;,]"))) {
        const auto rule = entry.trimmed().toLower();
        if (rule == "<-loopback>") {
            if (implicitBypass(url))
                result = false;
        } else if (!rule.isEmpty() && matches(url, rule)) {
            result = true;
        }
    }
    return result;
}
QList<NetworkProxyEndpoint> ProxyRouting::manual(const NetworkConfiguration &config,
                                                 const QUrl &url)
{
    if (bypasses(url, config.bypassRules))
        return {{}};
    const auto groups = NetworkConfiguration::parseProxyRules(config.proxyRules);
    if (!groups)
        return {};
    // Chromium chooses single-proxy versus mapped mode from the first group;
    // repeated mapped groups append, while later unqualified groups are ignored.
    if (groups->first().protocol.isEmpty())
        return groups->first().endpoints;
    QList<NetworkProxyEndpoint> matching, fallback, https, http;
    for (const auto &group : *groups) {
        if (group.protocol == url.scheme())
            matching += group.endpoints;
        if (group.protocol == "socks")
            fallback += group.endpoints;
        if (group.protocol == "https")
            https += group.endpoints;
        if (group.protocol == "http")
            http += group.endpoints;
    }
    if (url.scheme() == "ws" || url.scheme() == "wss")
        return !fallback.isEmpty() ? fallback
               : !https.isEmpty()  ? https
               : !http.isEmpty()   ? http
                                   : QList<NetworkProxyEndpoint>{{}};
    return !matching.isEmpty()   ? matching
           : !fallback.isEmpty() ? fallback
                                 : QList<NetworkProxyEndpoint>{{}};
}
QList<NetworkProxyEndpoint> ProxyRouting::pacResult(const QString &result)
{
    QList<NetworkProxyEndpoint> endpoints;
    for (const auto &part : result.split(';')) {
        const auto words = part.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (words.size() == 1 && words[0].compare("DIRECT", Qt::CaseInsensitive) == 0) {
            endpoints.append(NetworkProxyEndpoint{});
            continue;
        }
        if (words.size() != 2)
            continue;
        const auto type = words[0].toUpper();
        const QString scheme = type == "PROXY" || type == "HTTP"     ? "http"
                               : type == "HTTPS"                     ? "https"
                               : type == "SOCKS" || type == "SOCKS4" ? "socks4"
                               : type == "SOCKS5"                    ? "socks5"
                                                                     : QString();
        if (scheme.isEmpty())
            continue;
        const auto parsed = NetworkConfiguration::parseProxyRules(scheme + "://" + words[1]);
        if (parsed && parsed->size() == 1 && parsed->first().endpoints.size() == 1)
            endpoints.append(parsed->first().endpoints.first());
    }
    return endpoints;
}
} // namespace EchoMusic
