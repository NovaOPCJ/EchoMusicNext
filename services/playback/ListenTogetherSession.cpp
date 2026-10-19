#include "ListenTogetherController.h"
#include "PlaybackService.h"
#include "ListenTogetherSeed.h"
#include <QJsonArray>
#include <QSharedPointer>
#include <algorithm>
namespace EchoMusic
{
void ListenTogetherController::loadRooms(bool reset)
{
    if (!requireLogin() || !m_pageActive || !m_networkEnabled || m_loadingRooms ||
        (!reset && m_roomsEnded))
        return;
    m_loadingRooms = true;
    m_error.clear();
    emit changed();
    if (m_scope) {
        loadOwnedRooms();
        return;
    }
    ListenMusicRequest request;
    request.action = ListenMusicAction::List;
    request.page = reset ? 1 : m_page + 1;
    const int page = request.page;
    watch(
        m_api.listenTogetherMusic(request), Scope::Page,
        [this, reset, page](const QJsonObject &data) {
            if (reset)
                m_publicRooms.clear();
            QSet<QString> ids;
            for (const auto &row : m_publicRooms)
                ids.insert(row.toMap().value("id").toString());
            for (const auto &value : data.value("items").toArray()) {
                const auto room = value.toObject().toVariantMap();
                const auto id = room.value("id").toString();
                if (id.isEmpty() || !ids.contains(id)) {
                    m_publicRooms.append(room);
                    ids.insert(id);
                }
            }
            m_page = page;
            m_roomsEnded = data.value("ended").isBool() ? data.value("ended").toBool()
                                                        : data.value("items").toArray().size() < 20;
            m_loadingRooms = false;
            updateRoomsProjection();
        },
        [this](const QJsonObject &error) {
            m_loadingRooms = false;
            reportError(error, false);
        });
}
void ListenTogetherController::loadOwnedRooms()
{
    struct Load
    {
        int pending = 2;
        bool complete = true;
        QSet<QString> ids;
        QVariantList rooms;
    };
    auto state = QSharedPointer<Load>::create();
    const auto finish = [this, state] {
        if (--state->pending)
            return;
        m_mineComplete = state->complete;
        if (state->complete)
            m_ownedRooms = state->rooms;
        else
            for (const auto &room : state->rooms)
                upsertOwned(room.toMap());
        m_loadingRooms = false;
        m_roomsEnded = true;
        saveRoomIndex();
        updateRoomsProjection();
    };
    const auto inspect = [this, state, finish](const QString &id) {
        if (id.isEmpty() || state->ids.contains(id) || m_dissolved.contains(id))
            return;
        state->ids.insert(id);
        ++state->pending;
        ListenRoomRequest check;
        check.action = ListenRoomAction::State;
        check.roomId = id;
        watch(
            m_api.listenTogetherRoom(check), Scope::Page,
            [this, state, finish, id](const QJsonObject &status) {
                if (status.value("active_code").isDouble() &&
                    status.value("active_code").toInt() != 1) {
                    rememberDissolved(id);
                    finish();
                    return;
                }
                ListenMusicRequest detail;
                detail.action = ListenMusicAction::Detail;
                detail.roomId = id;
                watch(
                    m_api.listenTogetherMusic(detail), Scope::Page,
                    [this, state, finish](const QJsonObject &room) {
                        if (room.value("owner").toObject().value("id").toString() ==
                            QString::number(m_api.currentAccountUserId()))
                            state->rooms.append(room.toVariantMap());
                        finish();
                    },
                    [this, state, finish, id](const QJsonObject &error) {
                        if (dissolved(error))
                            rememberDissolved(id);
                        else
                            state->complete = false;
                        finish();
                    });
            },
            [this, state, finish, id](const QJsonObject &error) {
                if (dissolved(error))
                    rememberDissolved(id);
                else
                    state->complete = false;
                finish();
            });
    };
    ListenMusicRequest history;
    history.action = ListenMusicAction::History;
    watch(
        m_api.listenTogetherMusic(history), Scope::Page,
        [inspect, finish](const QJsonObject &data) {
            const auto items = data.value("items").toArray();
            for (int i = 0; i < qMin(20, int(items.size())); ++i)
                inspect(items[i].toObject().value("id").toString());
            finish();
        },
        [this, state, finish](const QJsonObject &error) {
            state->complete = false;
            reportError(error, false);
            finish();
        });
    ListenRoomRequest status;
    watch(
        m_api.listenTogetherRoom(status), Scope::Page,
        [inspect, finish](const QJsonObject &data) {
            inspect(data.value("room_id").toString());
            finish();
        },
        [state, finish](const QJsonObject &) {
            state->complete = false;
            finish();
        });
}
void ListenTogetherController::inspectRoom(const QString &id)
{
    if (!requireLogin() || id.isEmpty() || !m_pageActive)
        return;
    closePreview();
    m_loadingPreview = true;
    emit changed();
    struct Preview
    {
        int left = 3;
        QVariantMap detail;
        QVariantList members;
        bool valid = true;
        QJsonObject error;
    };
    auto result = QSharedPointer<Preview>::create();
    const auto done = [this, result, id] {
        if (--result->left)
            return;
        m_loadingPreview = false;
        if (!result->valid) {
            if (dissolved(result->error))
                rememberDissolved(id);
            reportError(result->error);
            emit previewClosed();
            return;
        }
        m_previewRoom = result->detail;
        m_previewMembers.replace(result->members);
        emit changed();
        emit previewRequested();
    };
    const auto error = [result, done](const QJsonObject &e) {
        result->valid = false;
        result->error = e;
        done();
    };
    ListenRoomRequest state;
    state.action = ListenRoomAction::State;
    state.roomId = id;
    watch(
        m_api.listenTogetherRoom(state), Scope::Preview,
        [result, done](const QJsonObject &data) {
            if (data.value("active_code").isDouble() && data.value("active_code").toInt() != 1) {
                result->valid = false;
                result->error = {{"code", 20005}, {"message", QStringLiteral("房间已解散")}};
            }
            done();
        },
        error);
    ListenMusicRequest request;
    request.action = ListenMusicAction::Detail;
    request.roomId = id;
    watch(
        m_api.listenTogetherMusic(request), Scope::Preview,
        [result, done](const QJsonObject &data) {
            result->detail = data.toVariantMap();
            done();
        },
        error);
    request.action = ListenMusicAction::Members;
    watch(
        m_api.listenTogetherMusic(request), Scope::Preview,
        [result, done](const QJsonObject &data) {
            result->members = data.value("items").toArray().toVariantList();
            done();
        },
        error);
}
void ListenTogetherController::closePreview()
{
    const bool hadPreview =
        m_loadingPreview || !m_previewRoom.isEmpty() || m_previewMembers.rowCount() > 0;
    ++m_previewRevision;
    cancel(Scope::Preview);
    m_loadingPreview = false;
    m_previewRoom.clear();
    m_previewMembers.clear();
    if (hadPreview) {
        emit changed();
        emit previewClosed();
    }
}
void ListenTogetherController::recover(Success done, Failure failed)
{
    ListenRoomRequest request;
    watch(
        m_api.listenTogetherRoom(request), Scope::Account,
        [this, done, failed](const QJsonObject &session) {
            const auto id = session.value("room_id").toString();
            if (id.isEmpty()) {
                done(session);
                return;
            }
            ListenMusicRequest detail;
            detail.action = ListenMusicAction::Detail;
            detail.roomId = id;
            watch(
                m_api.listenTogetherMusic(detail), Scope::Account,
                [done, session](const QJsonObject &room) {
                    auto merged = session;
                    merged.insert("detail", room);
                    done(merged);
                },
                [this, done, failed](const QJsonObject &error) {
                    if (dissolved(error))
                        done({});
                    else if (failed)
                        failed(error);
                });
        },
        failed);
}
void ListenTogetherController::transition(const QString &key, Phase phase,
                                          std::function<void()> action)
{
    if (!requireLogin())
        return;
    if (m_phase == Joining || m_phase == Creating || m_phase == Leaving)
        return;
    ++m_epoch;
    cancel(Scope::Account);
    m_transitionKey = key;
    m_createdName.clear();
    m_error.clear();
    m_phase = phase;
    emit changed();
    action();
}
void ListenTogetherController::createRoom(const QString &rawName, int privacy)
{
    const auto name = rawName.trimmed();
    if (name.isEmpty()) {
        emit notice(tr("请输入房间名称"), 1);
        return;
    }
    if (listenTogetherSeed(m_playback.queue()->tracks(), m_playback.currentTrack()).isEmpty()) {
        emit notice(tr("当前没有可用播放队列，请先播放一首歌"), 1);
        return;
    }
    if (owner()) {
        emit notice(tr("请先结束当前房间，再创建新房间"), 1);
        return;
    }
    if (joined()) {
        leaveInternal(false, [this, name, privacy] { createRoom(name, privacy); });
        return;
    }
    transition("create", Creating, [this, name, privacy] {
        m_createdName = name;
        m_initialTracks =
            listenTogetherSeed(m_playback.queue()->tracks(), m_playback.currentTrack());
        const auto &current = m_playback.currentTrack();
        m_initialProgress = !current.hash.trimmed().isEmpty() && current.songId
                                ? std::optional(currentProgress())
                                : std::nullopt;
        recover(
            [this, name, privacy](const QJsonObject &session) {
                const auto old = session.value("room_id").toString();
                if (!old.isEmpty()) {
                    hydrate(old, session.toVariantMap());
                    return;
                }
                ListenRoomRequest create;
                create.action = ListenRoomAction::Create;
                create.name = name;
                create.privacy = privacy == 2 ? 2 : 1;
                watch(
                    m_api.listenTogetherRoom(create), Scope::Account,
                    [this](const QJsonObject &data) {
                        const auto id = data.value("room_id").toString();
                        if (id.isEmpty()) {
                            m_phase = Error;
                            reportError({{"message", tr("创建房间未返回房间标识")}});
                            return;
                        }
                        ListenMusicRequest init;
                        init.action = ListenMusicAction::Initialize;
                        init.roomId = id;
                        init.progress = m_initialProgress;
                        for (const auto &track : m_initialTracks)
                            init.audios.append(trackAudio(track));
                        watch(
                            m_api.listenTogetherMusic(init), Scope::Account,
                            [this, id, data](const QJsonObject &) {
                                auto session = data.toVariantMap();
                                session.insert("owner", true);
                                hydrate(id, session);
                            },
                            [this, id](const QJsonObject &error) {
                                ListenRoomRequest cleanup;
                                cleanup.action = ListenRoomAction::Dismiss;
                                cleanup.roomId = id;
                                watch(
                                    m_api.listenTogetherRoom(cleanup), Scope::Account,
                                    [this, id, error](const QJsonObject &) {
                                        rememberDissolved(id);
                                        m_phase = Error;
                                        reportError(error);
                                    },
                                    [this, error](const QJsonObject &) {
                                        m_phase = Error;
                                        reportError(error);
                                    });
                            });
                    },
                    [this](const QJsonObject &error) {
                        if (error.value("code").toVariant().toInt() == 20006) {
                            recover(
                                [this, error](const QJsonObject &session) {
                                    const auto id = session.value("room_id").toString();
                                    if (!id.isEmpty())
                                        hydrate(id, session.toVariantMap());
                                    else {
                                        m_phase = Error;
                                        reportError(error);
                                    }
                                },
                                [this](const QJsonObject &e) {
                                    m_phase = Error;
                                    reportError(e);
                                });
                        } else {
                            m_phase = Error;
                            reportError(error);
                        }
                    });
            },
            [this](const QJsonObject &error) {
                m_phase = Error;
                reportError(error);
            });
    });
}
void ListenTogetherController::joinRoom(const QString &id)
{
    if (id.isEmpty())
        return;
    if (joined() && id == m_roomId) {
        closePreview();
        resumeRoomPlayback();
        emit sessionReady();
        return;
    }
    if (owner()) {
        emit notice(tr("请先结束当前房间，再加入其他房间"), 1);
        return;
    }
    if (joined()) {
        leaveInternal(false, [this, id] { joinRoom(id); });
        return;
    }
    transition(id, Joining, [this, id] {
        ListenRoomRequest state;
        state.action = ListenRoomAction::State;
        state.roomId = id;
        watch(
            m_api.listenTogetherRoom(state), Scope::Account,
            [this, id](const QJsonObject &data) {
                if (data.value("active_code").isDouble() &&
                    data.value("active_code").toInt() != 1) {
                    rememberDissolved(id);
                    m_phase = Error;
                    reportError({{"code", 20005}, {"message", tr("房间已解散")}});
                    return;
                }
                ListenRoomRequest join;
                join.action = ListenRoomAction::Join;
                join.roomId = id;
                watch(
                    m_api.listenTogetherRoom(join), Scope::Account,
                    [this, id, data](const QJsonObject &session) {
                        auto facts = session.toVariantMap();
                        facts.insert("owner_profile",
                                     data.value("owner_profile").toObject().toVariantMap());
                        hydrate(id, facts);
                    },
                    [this, id](const QJsonObject &error) {
                        if (dissolved(error))
                            rememberDissolved(id);
                        if (error.value("code").toVariant().toInt() == 20006) {
                            recover(
                                [this, id, error](const QJsonObject &session) {
                                    if (session.value("room_id").toString() == id)
                                        hydrate(id, session.toVariantMap());
                                    else {
                                        m_phase = Error;
                                        reportError(error);
                                    }
                                },
                                [this](const QJsonObject &e) {
                                    m_phase = Error;
                                    reportError(e);
                                });
                        } else {
                            m_phase = Error;
                            reportError(error);
                        }
                    });
            },
            [this, id](const QJsonObject &error) {
                if (dissolved(error))
                    rememberDissolved(id);
                m_phase = Error;
                reportError(error);
            });
    });
}
void ListenTogetherController::hydrate(const QString &id, const QVariantMap &session)
{
    m_previousQueue =
        m_playback.sourceKey().startsWith("queue:personal-fm") || m_playback.personalFm()->active()
            ? m_playback.queueCatalog()->lastLocalKey()
            : m_playback.sourceKey();
    m_roomId = id;
    m_activeRoom = session.value("detail").toMap();
    if (m_activeRoom.isEmpty())
        m_activeRoom = findRoom(id);
    m_activeRoom.insert("id", id);
    if (m_activeRoom.value("name").toString().isEmpty() && !m_createdName.isEmpty())
        m_activeRoom.insert("name", m_createdName);
    if (!session.value("owner_profile").toMap().isEmpty())
        m_activeRoom.insert("owner", session.value("owner_profile"));
    if (session.value("owner").toBool())
        m_activeRoom.insert("owner",
                            QVariantMap{{"id", QString::number(m_api.currentAccountUserId())}});
    m_phase = Joined;
    m_loadingRoom = true;
    m_initialSongsDone = m_initialSyncDone = false;
    m_detached = m_guestPaused = false;
    m_transitionKey.clear();
    closePreview();
    startTimers();
    loadSongs(true);
    syncPlayback(true, [this] {
        m_initialSyncDone = true;
        finishHydration();
    });
    loadRoomDetail();
    loadMembers();
    loadMessages();
    refreshOrders();
    emit changed();
}
void ListenTogetherController::finishHydration()
{
    if (!joined() || !m_initialSongsDone || !m_initialSyncDone)
        return;
    m_loadingRoom = false;
    applyRemote(true);
    emit changed();
    emit sessionReady();
}
void ListenTogetherController::leaveRoom(bool dismiss)
{
    if (joined())
        leaveInternal(dismiss);
}
void ListenTogetherController::leaveInternal(bool dismiss, std::function<void()> continuation)
{
    if (!joined() || (dismiss && !owner()))
        return;
    const auto id = m_roomId;
    m_phase = Leaving;
    ++m_epoch;
    cancel(Scope::Room);
    cancelMetadata();
    ++m_songGeneration;
    m_memberRequest = m_messageRequest = m_syncRequest = m_orderRequest = 0;
    m_loadingSongs = m_loadingOrders = m_sendingMessage = m_updatingChat = m_addingSongs = false;
    m_fastPending = m_slowPending = m_heartbeatPending = m_ownerPending = false;
    m_ownerCommands.clear();
    m_remoteApplying = m_remoteSeekPending = false;
    m_fastTimer.stop();
    m_slowTimer.stop();
    m_heartbeatTimer.stop();
    emit changed();
    ListenRoomRequest request;
    request.action = dismiss ? ListenRoomAction::Dismiss : ListenRoomAction::Leave;
    request.roomId = id;
    watch(
        m_api.listenTogetherRoom(request), Scope::Account,
        [this, id, dismiss, continuation](const QJsonObject &) {
            clearSession();
            if (dismiss)
                rememberDissolved(id);
            if (continuation)
                continuation();
            else if (m_pageActive)
                loadRooms();
        },
        [this, id, continuation](const QJsonObject &error) {
            if (dissolved(error)) {
                clearSession();
                rememberDissolved(id);
                if (continuation)
                    continuation();
            } else {
                m_phase = Joined;
                startTimers();
                loadSongs(true);
                syncPlayback(true);
                loadMembers();
                loadMessages();
                refreshOrders();
                reportError(error);
            }
        });
}
void ListenTogetherController::dismissOwnedRoom(const QString &id)
{
    if (id == m_roomId && owner()) {
        leaveRoom(true);
        return;
    }
    const auto room = findRoom(id);
    if (!requireLogin() || room.value("owner").toMap().value("id").toString() !=
                               QString::number(m_api.currentAccountUserId()))
        return;
    ListenRoomRequest request;
    request.action = ListenRoomAction::Dismiss;
    request.roomId = id;
    watch(
        m_api.listenTogetherRoom(request), Scope::Page,
        [this, id](const QJsonObject &) {
            rememberDissolved(id);
            closePreview();
        },
        [this, id](const QJsonObject &error) {
            if (dissolved(error))
                rememberDissolved(id);
            else
                reportError(error);
        });
}
void ListenTogetherController::clearSession(bool preserveTransition)
{
    disconnect(&m_playback, &PlaybackService::changed, this,
               &ListenTogetherController::playbackChanged);
    disconnect(&m_playback, &PlaybackService::positionChanged, this,
               &ListenTogetherController::positionChanged);
    cancelMetadata();
    m_fastTimer.stop();
    m_slowTimer.stop();
    m_heartbeatTimer.stop();
    cancel(Scope::Room);
    ++m_songGeneration;
    m_phase = Idle;
    restoreQueue();
    m_roomId.clear();
    m_activeRoom.clear();
    m_remote = {};
    m_tracks.clear();
    m_songPages = {};
    m_members.clear();
    m_songs.clear();
    m_messages.clear();
    m_orders.clear();
    m_memberSnapshot.clear();
    m_memberSnapshotReady = false;
    m_ownerCommands.clear();
    m_playbackRetry.clear();
    m_ownerAutomatic = false;
    m_roomFacts.clear();
    m_version.clear();
    m_songPageVersion.clear();
    m_lastMessageId = "0";
    m_loadingRoom = m_loadingSongs = m_loadingOrders = m_sendingMessage = m_updatingChat =
        m_addingSongs = false;
    m_fastPending = m_slowPending = m_heartbeatPending = m_ownerPending = m_remoteApplying =
        m_remoteSeekPending = false;
    m_memberRequest = m_messageRequest = m_syncRequest = m_orderRequest = 0;
    m_detached = m_guestPaused = m_forceAfterLoad = false;
    m_heartbeatFailures = 0;
    m_ownerGraceUntil = 0;
    m_remoteSeekLatency = 0;
    m_seekStarted = -1;
    m_pendingLocalSeek = -1;
    m_createdName.clear();
    m_lastTrackKey.clear();
    m_seekKey.clear();
    if (!preserveTransition) {
        m_phase = Idle;
        m_transitionKey.clear();
    }
    emit changed();
}
} // namespace EchoMusic
