#include "ApiClient.h"
#include <QJsonArray>
namespace EchoMusic
{
namespace
{
QJsonObject audioJson(const ListenRoomAudio &audio)
{
    return {{"hash", audio.hash}, {"song_id", audio.songId}};
}
QJsonObject progressJson(const ListenRoomProgress &progress)
{
    return {{"audio", audioJson(progress.audio)},
            {"position_ms", qMax(qint64(0), progress.positionMs)},
            {"playing", progress.playing},
            {"play_mode", progress.playMode}};
}
} // namespace
quint64 ApiClient::listenTogetherRoom(const ListenRoomRequest &request)
{
    const QStringList actions{"create",    "status", "join",    "state",
                              "heartbeat", "leave",  "dismiss", "update_chat"};
    QJsonObject query{{"action", actions.value(int(request.action))}};
    if (request.action == ListenRoomAction::Create) {
        query.insert("name", request.name);
        query.insert("privacy", request.privacy);
        query.insert("capacity", request.capacity);
    } else {
        query.insert("room_id", request.roomId);
        if (request.action == ListenRoomAction::UpdateChat)
            query.insert("enabled", request.chatEnabled);
    }
    return submit({{"operation", "listen_together_room"}, {"query", query}});
}
quint64 ApiClient::listenTogetherMusic(const ListenMusicRequest &request)
{
    const QStringList actions{"list",        "history",     "detail",          "members",
                              "initialize",  "sync",        "playback_url",    "switch",
                              "player",      "playlist",    "recent_playlist", "order_song",
                              "song_orders", "remove_song", "add_songs"};
    QJsonObject query{{"action", actions.value(int(request.action))}};
    if (request.action == ListenMusicAction::List) {
        query.insert("page", request.page);
        query.insert("page_size", request.pageSize);
        query.insert("tags", request.tags);
    } else if (request.action == ListenMusicAction::History) {
        query.insert("cursor", qint64(request.cursor));
    } else {
        query.insert("room_id", request.roomId);
        switch (request.action) {
        case ListenMusicAction::Members:
            query.insert("page_size", request.pageSize);
            break;
        case ListenMusicAction::Initialize:
        case ListenMusicAction::AddSongs: {
            QJsonArray audios;
            for (const auto &audio : request.audios)
                audios.append(audioJson(audio));
            query.insert("audios", audios);
            query.insert("progress", request.progress ? QJsonValue(progressJson(*request.progress))
                                                      : QJsonValue());
            if (request.action == ListenMusicAction::AddSongs) {
                query.insert("version", request.version);
                query.insert("requester_id", request.requesterId);
            }
            break;
        }
        case ListenMusicAction::PlaybackUrl:
        case ListenMusicAction::Switch:
        case ListenMusicAction::OrderSong:
        case ListenMusicAction::RemoveSong:
            if (!request.audio)
                return 0;
            query.insert("audio", audioJson(*request.audio));
            if (request.action == ListenMusicAction::Switch) {
                query.insert("version", request.version);
                query.insert("automatic", request.automatic);
            } else if (request.action == ListenMusicAction::RemoveSong) {
                query.insert("requester_id", request.requesterId);
            }
            break;
        case ListenMusicAction::Player: {
            QJsonObject command;
            switch (request.command) {
            case ListenPlayerCommand::Mode:
                command = {{"kind", "mode"}, {"mode", request.playMode}};
                break;
            case ListenPlayerCommand::Seek:
                command = {{"kind", "seek"}, {"position_ms", qMax(qint64(0), request.positionMs)}};
                break;
            case ListenPlayerCommand::Playing:
                command = {{"kind", "playing"}, {"playing", request.playing}};
                break;
            }
            query.insert("command", command);
            break;
        }
        case ListenMusicAction::Playlist:
            query.insert("cursor",
                         request.audio ? QJsonValue(audioJson(*request.audio)) : QJsonValue());
            break;
        default:
            break;
        }
    }
    return submit({{"operation", "listen_together_music"}, {"query", query}});
}
quint64 ApiClient::listenTogetherChat(const ListenChatRequest &request)
{
    QJsonObject query{{"action", request.action == ListenChatAction::Send ? "send" : "history"},
                      {"room_id", request.roomId}};
    if (request.action == ListenChatAction::Send) {
        query.insert("text", request.text);
        query.insert("nickname", request.nickname);
        query.insert("avatar_url", request.avatarUrl);
    } else {
        query.insert("max_id", request.maxId);
    }
    return submit({{"operation", "listen_together_chat"}, {"query", query}});
}
} // namespace EchoMusic
