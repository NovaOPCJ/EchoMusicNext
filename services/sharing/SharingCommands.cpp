#include "SharingCommands.h"
#include "AccountService.h"
#include <QClipboard>
#include <QGuiApplication>

namespace EchoMusic
{
QString playlistShareId(const QVariantMap &detail, const QString &fallback)
{
    const auto global = detail.value("global_id").toString().trimmed();
    const auto creatorGlobal = detail.value("creator_global_id").toString().trimmed();
    if (global.startsWith("collection_"))
        return global;
    if (creatorGlobal.startsWith("collection_"))
        return creatorGlobal;
    const auto user = detail.value("creator_id").toULongLong();
    const auto list = detail.value("creator_list_id").toULongLong();
    if (user && list)
        return QStringLiteral("collection_3_%1_%2_0").arg(user).arg(list);
    if (!global.isEmpty())
        return global;
    if (!creatorGlobal.isEmpty())
        return creatorGlobal;
    const auto id = detail.value("public_id").toULongLong();
    return id ? QString::number(id) : fallback.trimmed();
}
SharingCommands::SharingCommands(AccountService &account, QObject *parent)
    : QObject(parent), m_account(account)
{
}
QString SharingCommands::sharerName() const
{
    const auto name = m_account.nickname().trimmed();
    return name.isEmpty() ? QStringLiteral("EchoMusic") : name;
}
bool SharingCommands::copyTarget(const ShareTarget &target, const QString &title)
{
    const auto text = shareText(target, title, sharerName());
    if (text.isEmpty() || !qGuiApp)
        return false;
    emit copied(text);
    QGuiApplication::clipboard()->setText(text);
    emit notice(tr("分享链接已复制"), false);
    return true;
}
bool SharingCommands::shareSong(const QString &hash, const QString &title, quint64 albumId,
                                quint64 songId)
{
    ShareTarget target{ShareResource::Song, hash.trimmed().toLower(), {}};
    if (albumId)
        target.query.insert("albumId", QString::number(albumId));
    if (songId)
        target.query.insert("albumAudioId", QString::number(songId));
    return copyTarget(target, title);
}
bool SharingCommands::sharePlaylist(const QVariantMap &detail, const QString &fallback)
{
    return copyTarget({ShareResource::Playlist, playlistShareId(detail, fallback), {}},
                      detail.value("title").toString());
}
bool SharingCommands::shareArtist(const QString &id, const QString &title)
{
    return copyTarget({ShareResource::Artist, id.trimmed(), {}}, title);
}
bool SharingCommands::shareAlbum(const QString &id, const QString &title)
{
    return copyTarget({ShareResource::Album, id.trimmed(), {}}, title);
}
bool SharingCommands::shareRoom(const QString &id, const QString &title)
{
    return copyTarget({ShareResource::ListenTogether,
                       id.trimmed(),
                       {{"roomType", "0"}, {"roomName", title.trimmed()}}},
                      title);
}
} // namespace EchoMusic
