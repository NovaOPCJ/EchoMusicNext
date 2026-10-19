#include "SongCommands.h"
#include "AccountService.h"
#include "ContentBlacklistService.h"
#include "SongShareText.h"
#include "SharingCommands.h"
#include <QClipboard>
#include <QGuiApplication>
namespace EchoMusic
{
SongCommands::SongCommands(QObject *parent) : QObject(parent) {}
void SongCommands::attach(ContentBlacklistService &blacklist, AccountService &account)
{
    if (m_blacklist == &blacklist && m_account == &account)
        return;
    m_blacklist = &blacklist;
    m_account = &account;
    connect(&blacklist, &ContentBlacklistService::changed, this, &SongCommands::refresh);
    connect(&account, &AccountService::changed, this, &SongCommands::refresh);
    connect(&blacklist, &ContentBlacklistService::menuWriteFinished, this,
            [this](bool success, bool informational, const QString &message) {
                m_revision++;
                emit changed();
                emit notice(message, success ? (informational ? 0 : 1) : 2);
            });
    refresh();
}
bool SongCommands::busy() const
{
    return m_blacklist && m_blacklist->menuBusy();
}
void SongCommands::refresh()
{
    m_revision++;
    emit changed();
}
QString SongCommands::sharerName() const
{
    // fallback（源回退 EchoMusic）统一由 SongShareText 应用，避免两套默认值漂移。
    return m_account ? m_account->nickname().trimmed() : QString();
}
void SongCommands::attachSharing(SharingCommands &sharing)
{
    if (m_sharing == &sharing)
        return;
    if (m_sharing)
        disconnect(m_sharing, nullptr, this, nullptr);
    m_sharing = &sharing;
    connect(&sharing, &QObject::destroyed, this, &SongCommands::sharingChanged);
    emit sharingChanged();
}
SharingCommands *SongCommands::sharing() const
{
    return m_sharing;
}
bool SongCommands::share(const PlaybackTrack &track)
{
    if (m_sharing)
        return m_sharing->shareSong(track.hash, track.title, track.albumId,
                                    track.songId ? track.songId : track.id.toULongLong());
    const auto text = songShareText(track, sharerName());
    // 没有有效分享目标时静默返回。
    if (text.isEmpty())
        return false;
    if (!qGuiApp)
        return false;
    QGuiApplication::clipboard()->setText(text);
    emit notice(tr("分享链接已复制"), 1);
    return true;
}
bool SongCommands::copyInfo(const PlaybackTrack &track)
{
    // 标题缺失时使用歌手名；两者都为空则不复制，也不显示成功提示。
    const auto title = track.title.trimmed();
    const auto artist = track.artist.trimmed();
    const auto text = title.isEmpty()    ? artist
                      : artist.isEmpty() ? title
                                         : title + QStringLiteral(" - ") + artist;
    if (text.isEmpty() || !qGuiApp)
        return false;
    QGuiApplication::clipboard()->setText(text);
    emit notice(tr("歌曲信息已复制"), 1);
    return true;
}
bool SongCommands::blacklistAvailable(const PlaybackTrack &track) const
{
    return m_blacklist && m_account && m_account->userId() > 0 && isSongShareHash(track.hash);
}
bool SongCommands::blacklisted(const PlaybackTrack &track) const
{
    return m_blacklist &&
           m_blacklist->songStatus(track.hash) == ContentBlacklistService::SongStatus::Present;
}
bool SongCommands::addToBlacklist(const PlaybackTrack &track)
{
    if (!blacklistAvailable(track))
        return false;
    m_blacklist->requestSongBlacklist(
        track.hash,
        track.artist.isEmpty() ? track.title : track.artist + QStringLiteral(" - ") + track.title,
        track.id, false);
    refresh();
    return true;
}
bool SongCommands::removeFromBlacklist(const PlaybackTrack &track)
{
    if (!blacklistAvailable(track))
        return false;
    m_blacklist->requestSongBlacklist(
        track.hash,
        track.artist.isEmpty() ? track.title : track.artist + QStringLiteral(" - ") + track.title,
        track.id, true);
    refresh();
    return true;
}
} // namespace EchoMusic
