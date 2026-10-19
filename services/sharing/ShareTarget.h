#pragma once
#include <QMap>
#include <QString>
#include <QUrl>
#include <optional>

namespace EchoMusic
{
enum class ShareResource
{
    Song,
    Playlist,
    Artist,
    Album,
    Plugin,
    ListenTogether
};
struct ShareTarget
{
    ShareResource type;
    QString id;
    QMap<QString, QString> query;
    bool operator==(const ShareTarget &) const = default;
};
QString shareResourceName(ShareResource);
QString shareResourceLabel(ShareResource);
bool validShareTarget(const ShareTarget &);
QUrl shareAppUrl(const ShareTarget &);
QUrl shareWebUrl(const ShareTarget &);
QString shareText(const ShareTarget &, const QString &title = {}, const QString &sharer = {});
/// Supports typed app links, the known share landing pages and links inside
/// copied invitation text. It never resolves an arbitrary URL or starts playback.
std::optional<ShareTarget> parseShareTarget(const QString &);
} // namespace EchoMusic
