#include "ShareTarget.h"
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>

namespace EchoMusic
{
namespace
{
constexpr auto webBase = "https://hoowhoami.github.io/EchoMusicNext/share/";
std::optional<ShareResource> resource(const QString &value)
{
    for (auto kind : {ShareResource::Song, ShareResource::Playlist, ShareResource::Artist,
                      ShareResource::Album, ShareResource::Plugin, ShareResource::ListenTogether})
        if (shareResourceName(kind) == value)
            return kind;
    return std::nullopt;
}
QMap<QString, QString> cleanQuery(const QMap<QString, QString> &query)
{
    QMap<QString, QString> result;
    for (auto i = query.cbegin(); i != query.cend(); ++i)
        if (!i.key().trimmed().isEmpty() && !i.value().trimmed().isEmpty())
            result.insert(i.key().trimmed(), i.value().trimmed());
    return result;
}
QString encodedQuery(const QMap<QString, QString> &values)
{
    QByteArray result;
    for (auto i = values.cbegin(); i != values.cend(); ++i) {
        if (!result.isEmpty())
            result += '&';
        result += QUrl::toPercentEncoding(i.key()) + '=' + QUrl::toPercentEncoding(i.value());
    }
    return QString::fromLatin1(result);
}
std::optional<QMap<QString, QString>> readQuery(const QUrl &url, bool formEncoded = true)
{
    QMap<QString, QString> result;
    QSet<QString> seen;
    for (const auto &[rawKey, rawValue] : QUrlQuery(url).queryItems(QUrl::FullyEncoded)) {
        auto decode = [formEncoded](QString text) {
            if (formEncoded)
                text.replace('+', ' ');
            return QUrl::fromPercentEncoding(text.toUtf8());
        };
        const auto key = decode(rawKey);
        const auto value = decode(rawValue);
        // A link cannot give two different identities or sources to consumers.
        if (seen.contains(key))
            return std::nullopt;
        seen.insert(key);
        result.insert(key, value);
    }
    return cleanQuery(result);
}
QString stripPunctuation(QString value)
{
    value = value.trimmed();
    while (!value.isEmpty() && QStringLiteral(")]}>，。！？；,;>").contains(value.back()))
        value.chop(1);
    return value;
}
std::optional<ShareTarget> parseUrl(const QString &text, bool web = true)
{
    const QUrl url(stripPunctuation(text), QUrl::StrictMode);
    if (!url.isValid() || url.toEncoded().size() > 8192 || !url.userInfo().isEmpty() ||
        url.port() != -1 || url.hasFragment())
        return std::nullopt;
    const auto scheme = url.scheme();
    if (scheme == "echomusicnext" || scheme == "echomusic") {
        auto segments = url.path(QUrl::FullyEncoded).split('/', Qt::SkipEmptyParts);
        const auto kind = resource(url.host());
        if (!kind || segments.size() > 1)
            return std::nullopt;
        auto query = readQuery(url, !segments.isEmpty() || scheme == "echomusic");
        if (!query)
            return std::nullopt;
        QString id;
        if (!segments.isEmpty())
            id = QUrl::fromPercentEncoding(segments.first().toUtf8()).trimmed();
        else {
            // Already-issued native song/plugin links used ?id= rather than
            // a path. Receiving them does not register an old executable alias.
            id = query->take("id");
        }
        ShareTarget target{*kind, id, *query};
        return validShareTarget(target) ? std::optional(target) : std::nullopt;
    }
    if (!web || scheme != "https" || url.host() != "hoowhoami.github.io")
        return std::nullopt;
    QString path = url.path(QUrl::FullyEncoded);
    QString base;
    for (const auto &candidate :
         {QStringLiteral("/EchoMusicNext/share"), QStringLiteral("/EchoMusic/share")})
        if (path == candidate || path.startsWith(candidate + '/')) {
            base = candidate;
            break;
        }
    if (base.isEmpty())
        return std::nullopt;
    auto query = readQuery(url);
    if (!query)
        return std::nullopt;
    const auto nested = query->value("target");
    if (!nested.isEmpty())
        if (auto target = parseUrl(nested, false))
            return target;
    auto kind = resource(query->value("type"));
    QString id = query->value("id").trimmed();
    if (!kind || id.isEmpty()) {
        const auto parts = path.mid(base.size()).split('/', Qt::SkipEmptyParts);
        if (parts.size() != 2)
            return std::nullopt;
        kind = resource(parts.first());
        id = QUrl::fromPercentEncoding(parts.last().toUtf8()).trimmed();
    }
    if (!kind)
        return std::nullopt;
    query->remove("type");
    query->remove("id");
    query->remove("target");
    ShareTarget target{*kind, id, *query};
    return validShareTarget(target) ? std::optional(target) : std::nullopt;
}
} // namespace
QString shareResourceName(ShareResource type)
{
    switch (type) {
    case ShareResource::Song:
        return "song";
    case ShareResource::Playlist:
        return "playlist";
    case ShareResource::Artist:
        return "artist";
    case ShareResource::Album:
        return "album";
    case ShareResource::Plugin:
        return "plugin";
    case ShareResource::ListenTogether:
        return "listen-together";
    }
    return {};
}
QString shareResourceLabel(ShareResource type)
{
    switch (type) {
    case ShareResource::Song:
        return QStringLiteral("歌曲");
    case ShareResource::Playlist:
        return QStringLiteral("歌单");
    case ShareResource::Artist:
        return QStringLiteral("歌手");
    case ShareResource::Album:
        return QStringLiteral("专辑");
    case ShareResource::Plugin:
        return QStringLiteral("插件");
    case ShareResource::ListenTogether:
        return QStringLiteral("一起听房间");
    }
    return {};
}
bool validShareTarget(const ShareTarget &target)
{
    const auto id = target.id.trimmed();
    if (id.isEmpty() || id.size() > 512 || id.contains('/') || id.contains(QChar::Null))
        return false;
    if (target.type == ShareResource::Song)
        return QRegularExpression(QStringLiteral("^[a-fA-F0-9]{32}$")).match(id).hasMatch();
    if (target.type == ShareResource::Plugin)
        return QRegularExpression(QStringLiteral("^[a-zA-Z0-9._-]+$")).match(id).hasMatch();
    return !shareResourceName(target.type).isEmpty();
}
QUrl shareAppUrl(const ShareTarget &target)
{
    if (!validShareTarget(target))
        return {};
    QUrl url;
    url.setScheme("echomusicnext");
    url.setHost(shareResourceName(target.type));
    url.setPath('/' + target.id.trimmed());
    url.setQuery(encodedQuery(cleanQuery(target.query)), QUrl::StrictMode);
    return url.toEncoded().size() <= 4096 ? url : QUrl();
}
QUrl shareWebUrl(const ShareTarget &target)
{
    const auto app = shareAppUrl(target);
    if (app.isEmpty())
        return {};
    QUrl result(QString::fromLatin1(webBase));
    QMap<QString, QString> query;
    if (cleanQuery(target.query).isEmpty()) {
        query.insert("type", shareResourceName(target.type));
        query.insert("id", target.id.trimmed());
    } else
        query.insert("target", app.toString(QUrl::FullyEncoded));
    result.setQuery(encodedQuery(query), QUrl::StrictMode);
    return result;
}
QString shareText(const ShareTarget &target, const QString &title, const QString &sharer)
{
    const auto url = shareWebUrl(target);
    if (url.isEmpty())
        return {};
    return QStringLiteral("%1 给你分享了%2%3，快去看看吧\n%4")
        .arg(sharer.trimmed().isEmpty() ? QStringLiteral("EchoMusic") : sharer.trimmed(),
             shareResourceLabel(target.type),
             title.trimmed().isEmpty() ? QString() : QStringLiteral("「%1」").arg(title.trimmed()),
             url.toString(QUrl::FullyEncoded));
}
std::optional<ShareTarget> parseShareTarget(const QString &text)
{
    if (text.size() > 65536)
        return std::nullopt;
    if (auto target = parseUrl(text))
        return target;
    static const QRegularExpression links(
        QStringLiteral(R"((?:echomusicnext|echomusic)://[^\s<>"'`]+|https?://[^\s<>"'`]+)"),
        QRegularExpression::CaseInsensitiveOption);
    auto matches = links.globalMatch(text);
    while (matches.hasNext())
        if (auto target = parseUrl(matches.next().captured()))
            return target;
    return std::nullopt;
}
} // namespace EchoMusic
