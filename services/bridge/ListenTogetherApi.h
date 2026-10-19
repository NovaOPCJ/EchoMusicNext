#pragma once
#include <QList>
#include <QString>
#include <optional>
namespace EchoMusic
{
enum class ListenRoomAction
{
    Create,
    Status,
    Join,
    State,
    Heartbeat,
    Leave,
    Dismiss,
    UpdateChat
};
struct ListenRoomRequest
{
    ListenRoomAction action = ListenRoomAction::Status;
    QString roomId, name;
    int privacy = 1, capacity = 5;
    bool chatEnabled = true;
};
struct ListenRoomAudio
{
    QString hash, songId;
};
struct ListenRoomProgress
{
    ListenRoomAudio audio;
    qint64 positionMs = 0;
    bool playing = false;
    int playMode = 1;
};
enum class ListenMusicAction
{
    List,
    History,
    Detail,
    Members,
    Initialize,
    Sync,
    PlaybackUrl,
    Switch,
    Player,
    Playlist,
    RecentPlaylist,
    OrderSong,
    SongOrders,
    RemoveSong,
    AddSongs
};
enum class ListenPlayerCommand
{
    Mode,
    Seek,
    Playing
};
struct ListenMusicRequest
{
    ListenMusicAction action = ListenMusicAction::List;
    QString roomId, tags, version, requesterId;
    int page = 1, pageSize = 20;
    quint64 cursor = 0;
    QList<ListenRoomAudio> audios;
    std::optional<ListenRoomAudio> audio;
    std::optional<ListenRoomProgress> progress;
    ListenPlayerCommand command = ListenPlayerCommand::Playing;
    qint64 positionMs = 0;
    int playMode = 1;
    bool playing = false, automatic = false;
};
enum class ListenChatAction
{
    Send,
    History
};
struct ListenChatRequest
{
    ListenChatAction action = ListenChatAction::History;
    QString roomId, text, nickname, avatarUrl, maxId = "0";
};
} // namespace EchoMusic
