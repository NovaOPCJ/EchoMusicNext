#include "ApiClient.h"
#include "echo_ffi.h"

#include <QCoreApplication>
#include <QTimer>
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSet>
#include <limits>
#include <stdexcept>
#include <utility>

namespace EchoMusic
{
namespace
{
// This object has no parent: shutdown owns its lifetime beyond ApiClient deletion.
class Dispatcher final : public QObject
{
public:
    QPointer<ApiClient> receiver;
};
} // namespace

ApiClient::ApiClient(QObject *parent) : ApiClient(DeviceIdentity::temporary(), parent) {}

ApiClient::ApiClient(DeviceIdentity device, QObject *parent)
    : QObject(parent), m_device(std::move(device))
{
    auto dispatcher = new Dispatcher;
    dispatcher->receiver = this;
    m_dispatcher = dispatcher;
    initializeRuntime();
}
bool ApiClient::initializeRuntime()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_closing || echo_abi_version() != 1)
        return false;
    if (!m_runtime)
        m_runtime = echo_runtime_create();
    return m_runtime != nullptr;
}

ApiClient::~ApiClient()
{
    shutdown();
}

quint64 ApiClient::capabilities()
{
    return submit({{"operation", "capabilities"}}, false);
}
bool ApiClient::setNetworkTimeout(double seconds)
{
    Q_ASSERT(QThread::currentThread() == thread());
    return !m_closing && echo_runtime_api_timeout(m_runtime, seconds);
}
bool ApiClient::setNetworkTransport(quint16 loopbackPort)
{
    return !m_closing && echo_runtime_network_transport(m_runtime, loopbackPort);
}

quint64 ApiClient::submit(QJsonObject request, bool includeIdentity, RequestOptions options)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_closing)
        return 0;
    if (includeIdentity) {
        const auto &device = options.authentication == AuthenticationMode::IndependentDevice
                                 ? m_independentDevice
                                 : m_device;
        const bool account = options.authentication == AuthenticationMode::CurrentAccount;
        const QString mode = account ? QStringLiteral("current_account")
                             : options.authentication == AuthenticationMode::DeviceOnly
                                 ? QStringLiteral("device_only")
                                 : QStringLiteral("independent_device");
        request.insert(QStringLiteral("identity"),
                       QJsonObject{{"userid", account ? qint64(m_userId) : 0},
                                   {"token", account ? m_token : QString()},
                                   {"t1", account ? m_t1 : QString()},
                                   {"mid", device.mid},
                                   {"dfid", device.dfid},
                                   {"guid", device.guid},
                                   {"mac", device.mac},
                                   {"device_name", device.deviceName},
                                   {"uuid", device.uuid},
                                   {"render_fingerprint", device.renderFingerprint},
                                   {"authentication", mode}});
    }
    Pending pending;
    pending.revision = m_revision;
    pending.request = std::move(request);
    pending.automaticVerification = options.automaticVerification;
    pending.replayAfterVerification = options.replayAfterVerification;
    return submitPrepared(std::move(pending));
}

quint64 ApiClient::submitPrepared(Pending pending)
{
    if (m_closing || !m_runtime || pending.revision != m_revision)
        return 0;
    const QByteArray bytes = QJsonDocument(pending.request).toJson(QJsonDocument::Compact);
    const auto id =
        echo_runtime_submit(m_runtime, reinterpret_cast<const uint8_t *>(bytes.constData()),
                            size_t(bytes.size()), pending.revision, callback, m_dispatcher);
    if (id) {
        if (!pending.logicalId)
            pending.logicalId = id;
        m_pending.insert(id, std::move(pending));
    }
    return id;
}

quint64 ApiClient::refreshLogin()
{
    if (!m_userId || m_token.isEmpty())
        return 0;
    return submit({{"operation", "login_token"}}, true, {AuthenticationMode::CurrentAccount});
}
bool ApiClient::setDeviceIdentity(DeviceIdentity device)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_closing || device.guid.isEmpty() ||
        device.mid != DeviceIdentity::midForGuid(device.guid) || device.dfid.isEmpty() ||
        device.mac.isEmpty() || device.deviceName.isEmpty())
        return false;
    m_device = std::move(device);
    advanceSession(); // Device-bound old requests/challenges cannot reuse this revision.
    return true;
}
quint64 ApiClient::recognizeAudio(const QByteArray &pcm)
{
    if (pcm.isEmpty() || pcm.size() > 160000 || pcm.size() % 2)
        return 0;
    return submit({{"operation", "audio_match"},
                   {"query", QJsonObject{{"pcm_base64", QString::fromLatin1(pcm.toBase64())}}}});
}
quint64 ApiClient::registerDevice()
{
    return submit({{"operation", "register_dev"}}, true, {AuthenticationMode::DeviceOnly});
}
QString ApiClient::diagnosticAuthorization() const
{
    QStringList parts;
    const auto append = [&](const QString &key, const QString &value) {
        if (!value.isEmpty())
            parts.append(key + '=' + value);
    };
    append(QStringLiteral("token"), m_token);
    if (m_userId)
        append(QStringLiteral("userid"), QString::number(m_userId));
    append(QStringLiteral("t1"), m_t1);
    append(QStringLiteral("dfid"), m_device.dfid);
    append(QStringLiteral("KUGOU_API_MID"), m_device.mid);
    append(QStringLiteral("uuid"), m_device.uuid);
    append(QStringLiteral("KUGOU_API_GUID"), m_device.guid);
    append(QStringLiteral("KUGOU_API_DEV"), m_device.deviceName);
    append(QStringLiteral("KUGOU_API_MAC"), m_device.mac);
    return parts.join(';');
}
QVariantList ApiClient::diagnosticIdentityEntries() const
{
    QVariantList entries;
    const auto append = [&](const QString &label, const QString &value) {
        if (!value.isEmpty())
            entries.append(QVariantMap{{"label", label}, {"value", value}});
    };
    // Explicit laboratory diagnostics only; ordinary account properties stay credential-free.
    if (!m_token.isEmpty())
        append(QStringLiteral("token"), m_token);
    if (!m_t1.isEmpty())
        append(QStringLiteral("t1"), m_t1);
    append(QStringLiteral("dfid"), m_device.dfid);
    append(QStringLiteral("mid"), m_device.mid);
    append(QStringLiteral("uuid"), m_device.uuid);
    append(QStringLiteral("guid"), m_device.guid);
    append(QStringLiteral("dev"), m_device.deviceName);
    append(QStringLiteral("mac"), m_device.mac);
    return entries;
}
quint64 ApiClient::createQrLogin()
{
    return submit({{"operation", "login_qr_key"}}, true, {AuthenticationMode::DeviceOnly});
}
quint64 ApiClient::checkQrLogin(const QString &key)
{
    return submit({{"operation", "login_qr_check"}, {"query", QJsonObject{{"key", key}}}}, true,
                  {AuthenticationMode::DeviceOnly});
}

quint64 ApiClient::createProviderQr(QrProvider provider)
{
    if (provider == QrProvider::Kugou)
        return createQrLogin();
    return submit(
        {{"operation", provider == QrProvider::Qq ? "login_qq_qr_create" : "login_wx_create"}},
        false);
}
quint64 ApiClient::checkProviderQr(QrProvider provider, const QJsonObject &session)
{
    if (provider == QrProvider::Kugou)
        return checkQrLogin(session.value("key").toString());
    if (provider == QrProvider::Qq)
        return submit(
            {{"operation", "login_qq_qr_check"}, {"query", QJsonObject{{"session", session}}}},
            true, {AuthenticationMode::DeviceOnly});
    return submit({{"operation", "login_wx_check"}, {"query", session}}, false);
}
quint64 ApiClient::exchangeWechatCode(const QString &code)
{
    return submit({{"operation", "login_openplat"},
                   {"query", QJsonObject{{"stage", "authorize"}, {"code", code}}}},
                  true, {AuthenticationMode::DeviceOnly, false});
}
quint64 ApiClient::finishProviderLogin(QrProvider provider, const QJsonObject &authorization)
{
    if (provider == QrProvider::Kugou)
        return 0;
    QJsonObject query{{"authorization", authorization}};
    if (provider == QrProvider::Wechat)
        query.insert("stage", "finish");
    return submit({{"operation", provider == QrProvider::Wechat ? "login_openplat" : "login_qq"},
                   {"query", query}},
                  true, {AuthenticationMode::DeviceOnly});
}
quint64 ApiClient::sendSmsCode(const QString &mobile)
{
    return submit({{"operation", "captcha_sent"}, {"query", QJsonObject{{"mobile", mobile}}}}, true,
                  {AuthenticationMode::DeviceOnly});
}
quint64 ApiClient::smsLogin(const QString &mobile, const QString &code,
                            std::optional<quint64> selectedUser)
{
    QJsonObject query{{"mobile", mobile},
                      {"code", code},
                      {"user_id", selectedUser ? QJsonValue(qint64(*selectedUser))
                                               : QJsonValue(QJsonValue::Null)}};
    return submit({{"operation", "login_cellphone"}, {"query", query}}, true,
                  {AuthenticationMode::DeviceOnly});
}

quint64 ApiClient::passwordLogin(const QString &username, const QString &password)
{
    return submit({{"operation", "login"},
                   {"query", QJsonObject{{"username", username}, {"password", password}}}},
                  true, {AuthenticationMode::DeviceOnly, true});
}

quint64 ApiClient::searchHot(RequestOptions options)
{
    return submit({{"operation", "search_hot"}}, true, options);
}
quint64 ApiClient::searchDefault(RequestOptions options)
{
    return submit({{"operation", "search_default"}}, true, options);
}
quint64 ApiClient::searchSuggest(const QString &keyword, RequestOptions options)
{
    return submit({{"operation", "search_suggest"}, {"query", QJsonObject{{"keyword", keyword}}}},
                  true, options);
}
quint64 ApiClient::search(SearchKind kind, const QString &keyword, quint32 page, quint32 pageSize,
                          RequestOptions options)
{
    QString wireKind;
    switch (kind) {
    case SearchKind::Song:
        wireKind = QStringLiteral("song");
        break;
    case SearchKind::Album:
        wireKind = QStringLiteral("album");
        break;
    case SearchKind::Artist:
        wireKind = QStringLiteral("author");
        break;
    case SearchKind::Playlist:
        wireKind = QStringLiteral("special");
        break;
    case SearchKind::Lyric:
        wireKind = QStringLiteral("lyric");
        break;
    case SearchKind::Mv:
        wireKind = QStringLiteral("mv");
        break;
    }
    return submit({{"operation", "search"},
                   {"query", QJsonObject{{"keyword", keyword},
                                         {"kind", wireKind},
                                         {"page", qint64(page)},
                                         {"page_size", qint64(pageSize)}}}},
                  true, options);
}
quint64 ApiClient::recommendedPlaylists(const QString &categoryId)
{
    return submit(
        {{"operation", "top_playlist"}, {"query", QJsonObject{{"category_id", categoryId}}}});
}
quint64 ApiClient::editorPlaylists()
{
    return submit({{"operation", "top_ip"}});
}
quint64 ApiClient::styleRecommendations(const QStringList &tagIds)
{
    return submit({{"operation", "everyday_style_recommend"},
                   {"query", QJsonObject{{"tag_ids", QJsonArray::fromStringList(tagIds)}}}});
}
quint64 ApiClient::dailyRecommendations()
{
    return submit({{"operation", "everyday_recommend"}});
}
quint64 ApiClient::freeListenSongs()
{
    return submit({{"operation", "ai_recommend_song"}});
}
quint64 ApiClient::discoverRecommendations(quint32 pageSize)
{
    if (!pageSize)
        return 0;
    return submit(
        {{"operation", "home_discover"}, {"query", QJsonObject{{"page_size", int(pageSize)}}}},
        true, {AuthenticationMode::DeviceOnly, false});
}
quint64 ApiClient::personalFm(const FmRequest &request)
{
    const QStringList modes{"normal", "small", "peak", "radio"};
    const QStringList actions{"play",      "login",      "garbage",
                              "click_red", "cancel_red", "change_song_pool"};
    QJsonObject query{{"mode", modes.value(int(request.mode))},
                      {"action", actions.value(int(request.action))},
                      {"song_pool_id", int(request.pool)},
                      {"remain_songcnt", int(request.remaining)},
                      {"is_overplay", request.overplay},
                      {"hash", request.hash},
                      {"cur_mark", request.mark}};
    if (request.audioId)
        query.insert("songid", qint64(*request.audioId));
    if (request.playSeconds)
        query.insert("playtime", qint64(*request.playSeconds));
    // Explicit SSA completion permits one retry.
    // Transport failures do not authorize replaying this request.
    return submit({{"operation", "personal_fm"}, {"query", query}});
}
quint64 ApiClient::newSongCatalog()
{
    return submit({{"operation", "top_song"}}, true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::newAlbumCatalog()
{
    return submit({{"operation", "top_album"}}, true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::playlistCategories()
{
    return submit({{"operation", "playlist_tags"}}, true,
                  {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::artistDirectory(quint32 sex, quint32 category, quint32 musician)
{
    return submit({{"operation", "artist_lists"},
                   {"query", QJsonObject{{"sex", int(sex)},
                                         {"category", int(category)},
                                         {"musician", int(musician)}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::recommendedRanks()
{
    return submit({{"operation", "rank_top"}});
}
quint64 ApiClient::rankCatalog()
{
    return submit({{"operation", "rank_list"}});
}
quint64 ApiClient::rankSongs(quint64 rankId)
{
    return submit(
        {{"operation", "rank_audio"}, {"query", QJsonObject{{"rank_id", qint64(rankId)}}}});
}
quint64 ApiClient::songClimax(const QString &hash)
{
    return submit({{"operation", "song_climax"}, {"query", QJsonObject{{"hash", hash}}}}, true,
                  {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::albumDynamicCover(quint64 audio, quint64 album)
{
    if (audio > quint64(std::numeric_limits<qint64>::max()) ||
        album > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    return submit(
        {{"operation", "album_dycover"},
         {"query", QJsonObject{{"album_audio_id", qint64(audio)}, {"album_id", qint64(album)}}}},
        true, {AuthenticationMode::DeviceOnly, false});
}
namespace
{
QString commentResourceName(ApiClient::CommentResource resource)
{
    switch (resource) {
    case ApiClient::CommentResource::Album:
        return QStringLiteral("album");
    case ApiClient::CommentResource::Playlist:
        return QStringLiteral("playlist");
    case ApiClient::CommentResource::Music:
        return QStringLiteral("music");
    }
    Q_UNREACHABLE();
}
QJsonObject commentWriteQuery(const ApiClient::CommentWrite &w)
{
    QJsonObject q{{"resource", commentResourceName(w.resource)},
                  {"id", w.id},
                  {"name", w.name},
                  {"content", w.content},
                  {"comment_id", w.commentId},
                  {"thread_id", w.threadId},
                  {"parent_id", w.parentId},
                  {"reply_user_name", w.replyUserName},
                  {"reply_content", w.replyContent}};
    if (!w.code.isEmpty())
        q.insert("code", w.code);
    if (w.mixSongId)
        q.insert("mix_song_id", qint64(w.mixSongId));
    return q;
}
} // namespace
quint64 ApiClient::comments(const CommentQuery &q)
{
    QString operation;
    switch (q.section) {
    case CommentSection::Category:
        operation = "comment_music_classify";
        break;
    case CommentSection::HotWord:
        operation = "comment_music_hotword";
        break;
    case CommentSection::Floor:
        operation = "comment_floor";
        break;
    case CommentSection::All:
        operation = "comment_" + commentResourceName(q.resource);
        break;
    }
    QJsonObject query{{"resource", commentResourceName(q.resource)},
                      {"id", q.id},
                      {"page", qint64(q.page)},
                      {"page_size", qint64(q.pageSize)}};
    if (!q.code.isEmpty())
        query.insert("code", q.code);
    if (q.categoryId)
        query.insert("category_id", qint64(q.categoryId));
    if (q.mixSongId)
        query.insert("mix_song_id", qint64(q.mixSongId));
    if (!q.threadId.isEmpty())
        query.insert("thread_id", q.threadId);
    if (!q.hotWord.isEmpty())
        query.insert("hot_word", q.hotWord);
    return submit({{"operation", operation}, {"query", query}}, true,
                  {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::sendComment(const CommentWrite &w)
{
    return submit({{"operation", "comment_" + commentResourceName(w.resource) + "_send"},
                   {"query", commentWriteQuery(w)}},
                  true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::replyComment(const CommentWrite &w)
{
    return submit({{"operation", "comment_floor_send"}, {"query", commentWriteQuery(w)}}, true,
                  {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::deleteComment(const CommentWrite &w)
{
    return submit({{"operation", "comment_music_del"}, {"query", commentWriteQuery(w)}}, true,
                  {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::songRanking(quint64 mix, bool details)
{
    return submit(
        {{"operation", details ? "song_ranking_filter" : "song_ranking"},
         {"query", QJsonObject{{"album_audio_id", qint64(mix)}, {"page", 1}, {"page_size", 30}}}},
        true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::commentCount(const QString &hash)
{
    return submit({{"operation", "comment_count"}, {"query", QJsonObject{{"hash", hash}}}}, true,
                  {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::favoriteCount(quint64 mix)
{
    return submit(
        {{"operation", "favorite_count"}, {"query", QJsonObject{{"ids", QJsonArray{qint64(mix)}}}}},
        true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::songMv(quint64 mix)
{
    return submit(
        {{"operation", "kmr_audio_mv"}, {"query", QJsonObject{{"album_audio_id", qint64(mix)}}}},
        true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::videoDetail(const QString &id)
{
    return submit({{"operation", "video_detail"}, {"query", QJsonObject{{"id", id}}}}, true,
                  {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::videoPrivilege(const QString &hash)
{
    return submit({{"operation", "video_privilege"}, {"query", QJsonObject{{"hash", hash}}}}, true,
                  {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::videoUrl(const QString &hash)
{
    return submit({{"operation", "video_url"}, {"query", QJsonObject{{"hash", hash}}}}, true,
                  {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::collectVideo(quint64 id, bool collect)
{
    if (!m_userId || !id)
        return 0;
    return submit({{"operation", collect ? "mv_collect" : "mv_collect_del"},
                   {"query", QJsonObject{{"id", qint64(id)}}}},
                  true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::commentAuthorProducts(const QList<quint64> &ids)
{
    QJsonArray users;
    for (auto id : ids)
        users.append(qint64(id));
    return submit(
        {{"operation", "user_batch_union_vipinfo"}, {"query", QJsonObject{{"user_ids", users}}}},
        true, {AuthenticationMode::DeviceOnly, false});
}
quint64 ApiClient::songBarrage(const QString &hash, quint32 size)
{
    return submit(
        {{"operation", "song_barrage"},
         {"query", QJsonObject{{"hash", hash}, {"page", 1}, {"page_size", qint64(size)}}}},
        true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::sendSongBarrage(const QString &hash, const QString &name, const QString &content)
{
    return submit({{"operation", "song_barrage_send"},
                   {"query", QJsonObject{{"hash", hash}, {"name", name}, {"content", content}}}},
                  true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::videoBarrage(const QString &hash, quint32 size)
{
    return submit(
        {{"operation", "video_barrage"},
         {"query", QJsonObject{{"hash", hash}, {"page", 1}, {"page_size", qint64(size)}}}},
        true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::sendVideoBarrage(const QString &hash, const QString &name,
                                    const QString &content)
{
    return submit({{"operation", "video_barrage_send"},
                   {"query", QJsonObject{{"hash", hash}, {"name", name}, {"content", content}}}},
                  true, {AuthenticationMode::CurrentAccount, true});
}
quint64 ApiClient::albumDetail(quint64 albumId, RequestOptions options)
{
    if (albumId > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    return submit({{"operation", "album_detail"}, {"id", qint64(albumId)}}, true, options);
}
quint64 ApiClient::albumSongs(quint64 albumId, quint32 page, quint32 pageSize,
                              RequestOptions options)
{
    if (albumId > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    return submit({{"operation", "album_songs"},
                   {"query", QJsonObject{{"id", qint64(albumId)},
                                         {"page", qint64(page)},
                                         {"page_size", qint64(pageSize)}}}},
                  true, options);
}
quint64 ApiClient::artistDetail(quint64 artistId, RequestOptions options)
{
    if (artistId > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    return submit({{"operation", "artist_detail"}, {"id", qint64(artistId)}}, true, options);
}
quint64 ApiClient::artistSongs(quint64 artistId, quint32 page, quint32 pageSize,
                               const QString &sort, RequestOptions options)
{
    if (artistId > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    return submit({{"operation", "artist_songs"},
                   {"query", QJsonObject{{"id", qint64(artistId)},
                                         {"page", qint64(page)},
                                         {"page_size", qint64(pageSize)},
                                         {"sort", sort}}}},
                  true, options);
}
quint64 ApiClient::artistAlbums(quint64 artistId, quint32 page, quint32 pageSize,
                                const QString &sort, RequestOptions options)
{
    if (artistId > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    return submit({{"operation", "artist_albums"},
                   {"query", QJsonObject{{"id", qint64(artistId)},
                                         {"page", qint64(page)},
                                         {"page_size", qint64(pageSize)},
                                         {"sort", sort}}}},
                  true, options);
}
quint64 ApiClient::userDetail(RequestOptions options)
{
    return submit({{"operation", "user_detail"}}, true, options);
}
quint64 ApiClient::userInfo()
{
    return submit({{"operation", "user_info"}});
}
quint64 ApiClient::userVipDetail()
{
    return submit({{"operation", "user_vip_detail"}});
}
quint64 ApiClient::userGradeInfo(std::optional<quint64> userId, bool automaticVerification)
{
    if ((userId && !*userId) || (!userId && !m_userId))
        return 0;
    QJsonObject query{
        {"user_id", userId ? QJsonValue(qint64(*userId)) : QJsonValue(QJsonValue::Null)}};
    return submit({{"operation", "user_grade_info"}, {"query", query}}, true,
                  userId
                      ? RequestOptions{AuthenticationMode::DeviceOnly, false}
                      : RequestOptions{AuthenticationMode::CurrentAccount, automaticVerification});
}
quint64 ApiClient::reportListeningEvent(ListeningEvent event, quint64 songId, qint64 durationMs,
                                        bool completed)
{
    if (!m_userId || !songId || durationMs < 0)
        return 0;
    return submit(
        {{"operation", "user_listen_report"},
         {"query",
          QJsonObject{{"event", event == ListeningEvent::Start ? "start" : "end"},
                      {"mixsongid", qint64(songId)},
                      {"duration_ms", event == ListeningEvent::End ? QJsonValue(durationMs)
                                                                   : QJsonValue(QJsonValue::Null)},
                      {"completed", completed}}}},
        true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::reportListeningProgress(quint64 baselineSeconds, quint64 incrementSeconds)
{
    if (!m_userId)
        return 0;
    return submit({{"operation", "user_grade_progress"},
                   {"query", QJsonObject{{"baseline_seconds", qint64(baselineSeconds)},
                                         {"increment_seconds", qint64(incrementSeconds)}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::socialUsers(SocialList list)
{
    if (!m_userId)
        return 0;
    const QStringList operations{"user_follow", "user_friends", "user_fans", "user_visitors"};
    const auto value = int(list);
    if (value < 0 || value >= operations.size())
        return 0;
    return submit({{"operation", operations[value]}});
}
quint64 ApiClient::followUser(quint64 id, bool following)
{
    if (!m_userId || !id || id == m_userId)
        return 0;
    return submit(
        {{"operation", following ? "user_follow_add" : "user_follow_del"}, {"id", qint64(id)}},
        true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::followArtist(quint64 id, bool following)
{
    if (!m_userId || !id)
        return 0;
    return submit(
        {{"operation", following ? "artist_follow" : "artist_unfollow"}, {"id", qint64(id)}}, true,
        {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::socialMessages(quint64 target, const QString &tag)
{
    if (!m_userId || !target || target == m_userId)
        return 0;
    return submit({{"operation", "user_follow_message"},
                   {"query", QJsonObject{{"target", qint64(target)},
                                         {"tag", tag.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                               : QJsonValue(tag)}}}});
}
quint64 ApiClient::sendSocialMessage(quint64 target, const QString &tag, const QString &text,
                                     const QString &nickname)
{
    if (!m_userId || !target || target == m_userId || text.trimmed().isEmpty() || text.size() > 200)
        return 0;
    return submit({{"operation", "user_follow_chat"},
                   {"query", QJsonObject{{"target", qint64(target)},
                                         {"tag", tag.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                               : QJsonValue(tag)},
                                         {"text", text},
                                         {"nickname", nickname}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::loginDevices()
{
    return m_userId ? submit({{"operation", "login_device"}}) : 0;
}
quint64 ApiClient::kickLoginDevice(const LoginDeviceTarget &target)
{
    if (!m_userId || target.mid.isEmpty() || target.mid == m_device.mid ||
        !target.activeAtSeconds || !target.appId || !target.clientVersion)
        return 0;
    return submit({{"operation", "login_device_kick"},
                   {"query", QJsonObject{{"target_mid", target.mid},
                                         {"transport_fingerprint", target.transportFingerprint},
                                         {"active_at_seconds", qint64(target.activeAtSeconds)},
                                         {"app_id", qint64(target.appId)},
                                         {"client_version", qint64(target.clientVersion)}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::updateUserProfile(const ProfilePatch &patch)
{
    if (!m_userId)
        return 0;
    QJsonObject query;
    for (const auto &entry :
         {std::pair<QString, std::optional<QString>>{"nickname", patch.nickname},
          {"birthday", patch.birthday},
          {"signature", patch.signature},
          {"province", patch.province},
          {"city", patch.city}})
        if (entry.second)
            query.insert(entry.first, *entry.second);
    if (patch.gender)
        query.insert("gender", qint64(*patch.gender));
    if (query.isEmpty())
        return 0;
    return submit({{"operation", "user_update"}, {"query", query}}, true,
                  {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::updateUserAvatar(const QByteArray &image)
{
    if (!m_userId || image.isEmpty() || image.size() > 8 * 1024 * 1024)
        return 0;
    const auto id =
        submit({{"operation", "user_update_avatar"},
                {"query", QJsonObject{{"image_base64", QString::fromLatin1(image.toBase64())},
                                      {"local_date", QDate::currentDate().toString("yyyyMMdd")}}}},
               true, {AuthenticationMode::CurrentAccount, false});
    // This write is never replayed, and the native runtime already owns its
    // serialized input. Do not retain another image/base64 in pending metadata.
    if (id)
        m_pending[id].request = {{"operation", "user_update_avatar"}};
    return id;
}
quint64 ApiClient::contentBlacklist(BlacklistKind kind, quint32 page, quint32 pageSize)
{
    if (!m_userId || !page || !pageSize || pageSize > 500)
        return 0;
    return submit({{"operation", "blacklist_list"},
                   {"query", QJsonObject{{"label", kind == BlacklistKind::Song ? "song" : "singer"},
                                         {"page", qint64(page)},
                                         {"page_size", qint64(pageSize)}}}});
}
quint64 ApiClient::editContentBlacklist(BlacklistKind kind, const QString &key, const QString &name,
                                        const QString &mixSongId, bool remove)
{
    if (!m_userId || key.isEmpty())
        return 0;
    return submit({{"operation", "blacklist"},
                   {"query", QJsonObject{{"label", kind == BlacklistKind::Song ? "song" : "singer"},
                                         {"key", key},
                                         {"name", name},
                                         {"mix_song_id", mixSongId},
                                         {"remove", remove}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::listeningPreferences()
{
    return m_userId ? submit({{"operation", "user_preference"}}) : 0;
}
quint64 ApiClient::updateListeningPreferences(const PreferencePatch &patch)
{
    if (!m_userId)
        return 0;
    QJsonObject query;
    for (const auto &entry :
         {std::pair<QString, std::optional<QString>>{"recommendation_mode", patch.mode},
          {"gender", patch.gender},
          {"age", patch.age},
          {"languages", patch.languages},
          {"styles", patch.styles},
          {"recommendation", patch.recommendation}})
        if (entry.second)
            query.insert(entry.first, *entry.second);
    if (query.isEmpty())
        return 0;
    return submit({{"operation", "user_preference_update"}, {"query", query}}, true,
                  {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::userPlaylists(quint32 page, quint32 pageSize)
{
    if (!m_userId || !page || !pageSize || pageSize > 200)
        return 0;
    return submit(
        {{"operation", "user_playlist"},
         {"query", QJsonObject{{"page", qint64(page)}, {"page_size", qint64(pageSize)}}}});
}
quint64 ApiClient::importPlaylistLink(const QString &text)
{
    if (!m_userId || text.isEmpty() || text.size() > 8192)
        return 0;
    return submit({{"operation", "import_playlist"},
                   {"query", QJsonObject{{"action", "link"}, {"url", text}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::submitPlaylistImportImage(const QString &serial, const QByteArray &image)
{
    if (!m_userId || serial.isEmpty() || image.isEmpty() || image.size() > 10 * 1024 * 1024)
        return 0;
    return submit({{"operation", "import_playlist"},
                   {"query", QJsonObject{{"action", "submit_image"},
                                         {"task_sn", serial},
                                         {"image_base64", QString::fromLatin1(image.toBase64())}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::importPlaylistScreenshot(const QString &serial, CloudPlaylistId id,
                                            const QString &title)
{
    if (!m_userId || !id.value || serial.isEmpty() || title.isEmpty())
        return 0;
    return submit({{"operation", "import_playlist"},
                   {"query", QJsonObject{{"action", "screenshot"},
                                         {"task_sn", serial},
                                         {"cloud_list_id", qint64(id.value)},
                                         {"title", title}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::playlistImportStatus(quint64 task)
{
    if (!m_userId || !task)
        return 0;
    return submit(
        {{"operation", "import_playlist"},
         {"query", QJsonObject{{"action", "status"}, {"task_ids", QJsonArray{qint64(task)}}}}},
        true);
}
quint64 ApiClient::playlistImportResult(CloudPlaylistId id, quint32 page, quint32 size)
{
    if (!m_userId || !id.value || !page || !size || size > 100)
        return 0;
    return submit({{"operation", "import_playlist"},
                   {"query", QJsonObject{{"action", "result"},
                                         {"cloud_list_id", qint64(id.value)},
                                         {"page", qint64(page)},
                                         {"page_size", qint64(size)}}}},
                  true);
}
quint64 ApiClient::userCloud(quint32 page, quint32 pageSize)
{
    if (!m_userId || !page || pageSize > 100)
        return 0;
    return submit({{"operation", "user_cloud"},
                   {"query", QJsonObject{{"page", int(page)}, {"page_size", int(pageSize)}}}});
}
quint64 ApiClient::cloudMatch(const QString &hash)
{
    return m_userId
               ? submit({{"operation", "user_cloud_match"}, {"query", QJsonObject{{"hash", hash}}}},
                        true, {AuthenticationMode::CurrentAccount, true, false})
               : 0;
}
quint64 ApiClient::cloudUpload(CloudUploadStage stage, QJsonObject parameters)
{
    static const QStringList stages = {"auth", "initiate", "part", "complete", "add"};
    parameters.insert("stage", stages.at(int(stage)));
    return m_userId ? submit({{"operation", "user_cloud_upload"}, {"query", parameters}}, true,
                             {AuthenticationMode::CurrentAccount, true, false})
                    : 0;
}
quint64 ApiClient::userCloudDelete(const QList<CloudDeleteTarget> &targets)
{
    if (!m_userId || targets.isEmpty() || targets.size() > 200)
        return 0;
    QJsonArray files;
    for (const auto &target : targets) {
        if (!target.kvId || target.kvId > quint64(std::numeric_limits<qint64>::max()))
            return 0;
        QJsonObject file{{"kv_id", qint64(target.kvId)}};
        if (target.albumAudioId &&
            target.albumAudioId <= quint64(std::numeric_limits<qint64>::max()))
            file.insert("album_audio_id", qint64(target.albumAudioId));
        files.append(file);
    }
    return submit({{"operation", "user_cloud_del"}, {"query", QJsonObject{{"files", files}}}});
}
quint64 ApiClient::userVideoCollect(quint32 page, quint32 pageSize)
{
    if (!m_userId || !page || pageSize > 100)
        return 0;
    return submit({{"operation", "user_video_collect"},
                   {"query", QJsonObject{{"page", int(page)}, {"page_size", int(pageSize)}}}});
}
quint64 ApiClient::purchasedSongs(quint32 page, quint32 pageSize)
{
    if (!m_userId || !page || pageSize > 100)
        return 0;
    return submit({{"operation", "user_purchased_songs"},
                   {"query", QJsonObject{{"page", int(page)}, {"page_size", int(pageSize)}}}});
}
quint64 ApiClient::purchasedAlbums(quint32 page, quint32 pageSize)
{
    if (!m_userId || !page || pageSize > 100)
        return 0;
    return submit({{"operation", "user_purchased_albums"},
                   {"query", QJsonObject{{"page", int(page)}, {"page_size", int(pageSize)}}}});
}
quint64 ApiClient::playlistDetails(const QList<PublicCollectionId> &ids, RequestOptions options)
{
    if (ids.isEmpty() || ids.size() > 100)
        return 0;
    QJsonArray values;
    for (const auto &id : ids) {
        if (id.value.isEmpty() || id.value.size() > 256 || id.value.contains(u','))
            return 0;
        values.append(id.value);
    }
    return submit(
        {{"operation", "playlist_detail"}, {"query", QJsonObject{{"global_ids", values}}}}, true,
        options);
}
quint64 ApiClient::publicPlaylistTracks(const PublicCollectionId &id, quint32 page,
                                        quint32 pageSize, RequestOptions options)
{
    if (id.value.isEmpty() || id.value.size() > 256 || !page || !pageSize || pageSize > 200)
        return 0;
    return submit({{"operation", "playlist_track_all"},
                   {"query", QJsonObject{{"global_id", id.value},
                                         {"page", qint64(page)},
                                         {"page_size", qint64(pageSize)}}}},
                  true, options);
}
quint64 ApiClient::cloudPlaylistTracks(CloudPlaylistId id, quint32 page, quint32 pageSize)
{
    if (!m_userId || !id.value || id.value > quint64(std::numeric_limits<qint64>::max()) || !page ||
        !pageSize || pageSize > 300)
        return 0;
    return submit({{"operation", "playlist_track_all_new"},
                   {"query", QJsonObject{{"cloud_list_id", qint64(id.value)},
                                         {"page", qint64(page)},
                                         {"page_size", qint64(pageSize)}}}});
}
quint64 ApiClient::songMetadata(const QList<quint64> &ids, RequestOptions options)
{
    if (ids.isEmpty() || ids.size() > 100)
        return 0;
    QJsonArray values;
    QSet<quint64> seen;
    for (auto id : ids) {
        if (!id || id > quint64(std::numeric_limits<qint64>::max()) || seen.contains(id))
            return 0;
        seen.insert(id);
        values.append(qint64(id));
    }
    return submit({{"operation", "krm_audio"}, {"query", QJsonObject{{"album_audio_ids", values}}}},
                  true, options);
}
quint64 ApiClient::audioEffects(EffectCategory category, quint32 page, quint32 pageSize,
                                quint32 sort, quint64 brand)
{
    if (!page || !pageSize || pageSize > 50)
        return 0;
    static const QStringList operations{"effects_artist", "effects_brand", "effects_brand_detail",
                                        "get_model"};
    const auto index = int(category);
    if (index < 0 || index >= operations.size())
        return 0;
    QJsonObject query{{"page", int(page)}, {"page_size", int(pageSize)}};
    if (category == EffectCategory::Market) {
        if (sort < 2 || sort > 4)
            return 0;
        query.insert("sort", int(sort));
    }
    if (category == EffectCategory::HeadphoneModels) {
        if (!brand || brand > quint64(std::numeric_limits<qint64>::max()))
            return 0;
        query.insert("brand_id", qint64(brand));
    }
    return submit({{"operation", operations[index]}, {"query", query}}, true);
}
quint64 ApiClient::commonHeadphoneEffect()
{
    return submit({{"operation", "effects_match"}}, true);
}
quint64 ApiClient::lyricCandidates(const QString &hash, quint64 songId, quint64 durationMs,
                                   bool allCandidates)
{
    return submit({{"operation", "search_lyric"},
                   {"query", QJsonObject{{"hash", hash},
                                         {"song_id", qint64(songId)},
                                         {"duration_ms", qint64(durationMs)},
                                         {"all_candidates", allCandidates}}}});
}
quint64 ApiClient::lyric(const QString &id, const QString &accessKey)
{
    return submit(
        {{"operation", "lyric"}, {"query", QJsonObject{{"id", id}, {"accesskey", accessKey}}}});
}
quint64 ApiClient::lyricCandidatesByKeywords(const QString &keywords, quint64 durationMs,
                                             bool allCandidates)
{
    if (keywords.trimmed().isEmpty())
        return 0;
    return submit({{"operation", "search_lyric"},
                   {"query", QJsonObject{{"keywords", keywords.trimmed()},
                                         {"duration_ms", qint64(durationMs)},
                                         {"all_candidates", allCandidates}}}});
}
quint64 ApiClient::userVerify(RequestOptions options)
{
    return submit({{"operation", "user_verify"}}, true, options);
}
quint64 ApiClient::createPlaylist(const QString &title, bool isPrivate)
{
    if (!m_userId || title.trimmed().isEmpty())
        return 0;
    return submit({{"operation", "playlist_add"},
                   {"query", QJsonObject{{"title", title}, {"private", isPrivate}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::collectPlaylist(const CollectionTarget &target)
{
    if (!m_userId || target.title.trimmed().isEmpty() || !target.creatorListId)
        return 0;
    QJsonObject collection{{"source", int(target.source)},
                           {"creator_id", target.creatorId ? QJsonValue(qint64(*target.creatorId))
                                                           : QJsonValue(QJsonValue::Null)},
                           {"creator_list_id", qint64(target.creatorListId)},
                           {"creator_global_id", target.creatorGlobalId}};
    return submit(
        {{"operation", "playlist_add"},
         {"query",
          QJsonObject{{"title", target.title}, {"private", false}, {"collection", collection}}}},
        true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::addPlaylistTracks(CloudPlaylistId id, const QList<PlaylistTrackInput> &tracks)
{
    if (!m_userId || !id.value || id.value > quint64(std::numeric_limits<qint64>::max()) ||
        tracks.isEmpty() || tracks.size() > 300)
        return 0;
    QJsonArray values;
    for (const auto &track : tracks) {
        if (track.albumId > quint64(std::numeric_limits<qint64>::max()) ||
            track.songId > quint64(std::numeric_limits<qint64>::max()))
            return 0;
        values.append(QJsonObject{{"title", track.title},
                                  {"hash", track.hash},
                                  {"album_id", qint64(track.albumId)},
                                  {"song_id", qint64(track.songId)}});
    }
    return submit({{"operation", "playlist_tracks_add"},
                   {"query", QJsonObject{{"cloud_list_id", qint64(id.value)}, {"tracks", values}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::removePlaylistTracks(CloudPlaylistId id, const QList<PlaylistTrackFileId> &files)
{
    if (!m_userId || !id.value || id.value > quint64(std::numeric_limits<qint64>::max()) ||
        files.isEmpty() || files.size() > 300)
        return 0;
    QJsonArray values;
    for (auto file : files) {
        if (!file.value || file.value > quint64(std::numeric_limits<qint64>::max()))
            return 0;
        values.append(qint64(file.value));
    }
    return submit(
        {{"operation", "playlist_tracks_del"},
         {"query", QJsonObject{{"cloud_list_id", qint64(id.value)}, {"file_ids", values}}}},
        true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::sortPlaylistTracks(CloudPlaylistId id, quint64 revision,
                                      const QList<PlaylistTrackPosition> &positions)
{
    // Allow ordering across all loaded pages. Reject empty or duplicate IDs
    // without imposing a single-page count limit.
    if (!m_userId || !id.value || id.value > quint64(std::numeric_limits<qint64>::max()) ||
        revision > quint64(std::numeric_limits<qint64>::max()) || positions.isEmpty())
        return 0;
    QJsonArray values;
    for (auto position : positions) {
        if (!position.fileId.value ||
            position.fileId.value > quint64(std::numeric_limits<qint64>::max()))
            return 0;
        values.append(QJsonObject{{"file_id", qint64(position.fileId.value)},
                                  {"position", qint64(position.position)}});
    }
    return submit({{"operation", "playlist_tracks_sort"},
                   {"query", QJsonObject{{"cloud_list_id", qint64(id.value)},
                                         {"revision", qint64(revision)},
                                         {"positions", values}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::sortPlaylists(quint64 totalRevision, const QList<PlaylistPosition> &positions)
{
    if (!m_userId || totalRevision > quint64(std::numeric_limits<qint64>::max()) ||
        positions.isEmpty())
        return 0;
    QJsonArray values;
    QSet<quint64> ids;
    QSet<QPair<int, quint32>> occupiedPositions;
    for (const auto &p : positions) {
        if ((p.kind != PlaylistKind::Owned && p.kind != PlaylistKind::Collected) || !p.id.value ||
            p.id.value > quint64(std::numeric_limits<qint64>::max()) || ids.contains(p.id.value) ||
            occupiedPositions.contains({int(p.kind), p.position}))
            return 0;
        ids.insert(p.id.value);
        occupiedPositions.insert({int(p.kind), p.position});
        values.append(QJsonObject{{"cloud_list_id", qint64(p.id.value)},
                                  {"kind", p.kind == PlaylistKind::Owned ? "owned" : "collected"},
                                  {"position", qint64(p.position)}});
    }
    return submit(
        {{"operation", "playlist_sort"},
         {"query", QJsonObject{{"total_revision", qint64(totalRevision)}, {"positions", values}}}},
        true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::uploadPlaylistCover(const QByteArray &png)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_closing)
        return 0;
    if (png.size() > 8 * 1024 * 1024 || !png.startsWith(QByteArray::fromHex("89504e470d0a1a0a")))
        return 0;
    const auto date = QDate::currentDate().toString(QStringLiteral("yyyyMMdd")).toUInt();
    Pending pending;
    pending.revision = m_revision;
    pending.automaticVerification = false;
    // Metadata only: no image bytes/base64 retained in Qt's pending requests.
    pending.request = {{"operation", "playlist_pic_upload"}, {"local_date", qint64(date)}};
    const auto id = echo_runtime_upload_playlist_cover(
        m_runtime, reinterpret_cast<const uint8_t *>(png.constData()), size_t(png.size()), date,
        m_revision, callback, m_dispatcher);
    if (id) {
        pending.logicalId = id;
        m_pending.insert(id, std::move(pending));
    }
    return id;
}
quint64 ApiClient::updatePlaylist(const PlaylistUpdate &update)
{
    if (!m_userId || !update.id.value ||
        update.id.value > quint64(std::numeric_limits<qint64>::max()) ||
        update.totalRevision > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    QJsonObject query{{"cloud_list_id", qint64(update.id.value)},
                      {"total_revision", qint64(update.totalRevision)},
                      {"sort_index", update.sortIndex},
                      {"description", update.description},
                      {"tags", update.tags}};
    if (update.title)
        query.insert(QStringLiteral("title"), *update.title);
    if (update.coverReference)
        query.insert(QStringLiteral("cover_reference"), *update.coverReference);
    return submit({{"operation", "playlist_update"}, {"query", query}}, true,
                  {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::deletePlaylist(CloudPlaylistId id)
{
    if (!m_userId || !id.value || id.value > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    return submit({{"operation", "playlist_del"},
                   {"query", QJsonObject{{"cloud_list_id", qint64(id.value)}}}},
                  true, {AuthenticationMode::CurrentAccount, false});
}
quint64 ApiClient::playbackSource(const QString &hash, quint64 albumId, quint64 songId,
                                  PlaybackQuality quality, const QString &authorization,
                                  RequestOptions options, SoundEffect effect,
                                  const QString &originPage)
{
    if (albumId > quint64(std::numeric_limits<qint64>::max()) ||
        songId > quint64(std::numeric_limits<qint64>::max()))
        return 0;
    static const QStringList qualities{"standard",   "higher",      "flac",        "hi_res",
                                       "viper_tape", "viper_clear", "viper_atmos", "automatic"};
    const int q = int(quality);
    if (q < 0 || q >= qualities.size())
        return 0;
    static const QStringList effects{"none",      "piano",   "vocal",  "accompaniment",
                                     "subwoofer", "ancient", "surnay", "dj"};
    if (int(effect) < 0 || int(effect) >= effects.size())
        return 0;
    QJsonObject query{{"effect", effects[int(effect)]},
                      {"hash", hash},
                      {"album_id", qint64(albumId)},
                      {"song_id", qint64(songId)},
                      {"quality", qualities[q]}};
    if (!authorization.isEmpty())
        query.insert(QStringLiteral("authorization"), authorization);
    if (!originPage.isEmpty())
        query.insert(QStringLiteral("origin_page"), originPage);
    return submit({{"operation", "song_url_auth_merge"}, {"query", query}}, true, options);
}
quint64 ApiClient::songPrivileges(const QList<PrivilegeResource> &resources, RequestOptions options)
{
    QJsonArray items;
    for (const auto &resource : resources) {
        QJsonObject item{{"hash", resource.hash}};
        if (resource.albumId) {
            if (*resource.albumId > quint64(std::numeric_limits<qint64>::max()))
                return 0;
            item.insert(QStringLiteral("album_id"), qint64(*resource.albumId));
        }
        items.append(item);
    }
    return submit({{"operation", "privilege_lite"}, {"query", QJsonObject{{"resources", items}}}},
                  true, options);
}
bool ApiClient::matchesAccountSnapshot(quint64 userId, const QString &token,
                                       const QString &t1) const
{
    return !m_closing && userId == m_userId && token == m_token && t1 == m_t1;
}

bool ApiClient::setAccount(quint64 userId, QString token, QString t1)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_closing || userId > quint64(std::numeric_limits<qint64>::max()) ||
        (userId == 0) != token.isEmpty() || (userId == 0 && !t1.isEmpty()))
        return false;
    if (userId == m_userId && token == m_token && t1 == m_t1 && m_loginConfirmation.isEmpty())
        return true;
    auto confirmations = std::exchange(m_loginConfirmation, {});
    m_userId = userId;
    m_token = std::move(token);
    m_t1 = std::move(t1);
    advanceSession(); // Direct dismissal slots now see the new account snapshot.
    const auto revision = m_revision;
    // Rebuild identity only for the explicitly confirmed login event. Posting
    // lets AccountService and all model reset listeners finish before replay.
    QTimer::singleShot(
        0, this, [this, revision, userId, confirmations = std::move(confirmations)]() mutable {
            if (m_closing || m_revision != revision || !userId)
                return;
            for (auto pending : std::as_const(confirmations)) {
                const auto previous =
                    pending.request.value("identity").toObject().value("userid").toInteger();
                if (previous > 0 && quint64(previous) != userId) {
                    emit failed(pending.logicalId, EchoStatus::Cancelled,
                                QByteArrayLiteral("{\"kind\":\"cancelled\"}"));
                    continue;
                }
                pending.revision = revision;
                pending.retried = true;
                pending.verifiesRequest = 0;
                auto identity = pending.request.value("identity").toObject();
                if (identity.value("authentication") == QStringLiteral("current_account")) {
                    identity.insert("userid", qint64(m_userId));
                    identity.insert("token", m_token);
                    identity.insert("t1", m_t1);
                }
                pending.request.insert("identity", identity);
                if (!submitPrepared(pending))
                    emit failed(pending.logicalId, EchoStatus::Closed,
                                QByteArrayLiteral("{\"kind\":\"closed\"}"));
            }
        });
    return true;
}

void ApiClient::callback(void *context, const EchoResult *result) noexcept
{
    auto dispatcher = static_cast<Dispatcher *>(context);
    const QByteArray bytes(reinterpret_cast<const char *>(result->payload),
                           qsizetype(result->payload_len));
    const auto id = result->request_id;
    const auto revision = result->session_revision;
    const int status = int(result->status);
    // Access the QPointer only on its QObject thread. Rust drain keeps dispatcher
    // alive during invokeMethod; Qt removes posted events when it is later deleted.
    QMetaObject::invokeMethod(
        dispatcher,
        [dispatcher, id, revision, status, bytes] {
            if (dispatcher->receiver)
                dispatcher->receiver->deliver(id, revision, status, bytes);
        },
        Qt::QueuedConnection);
}

void ApiClient::deliver(quint64 id, quint64 revision, int status, const QByteArray &payload)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (!m_pending.contains(id) || m_closing || revision != m_revision)
        return;
    auto pending = m_pending.take(id);
    const auto original = pending.logicalId;
    const auto error = QJsonDocument::fromJson(payload).object();
    // Canonical metadata only: never log query/identity, response bodies,
    // provider messages, account/device ids or CAPTCHA secrets.
    const auto category =
        QByteArray("echo.api.") + pending.request.value("operation").toString().toLatin1();
    const auto code = error.value("code");
    const bool routine = status == EchoStatus::Success || status == EchoStatus::Cancelled;
    QString summary = status == EchoStatus::Success     ? QStringLiteral("Request completed")
                      : status == EchoStatus::Cancelled ? QStringLiteral("Request cancelled")
                                                        : QStringLiteral("Request failed");
    summary += QStringLiteral(" request=%1 session=%2").arg(original).arg(revision);
    if (id != original)
        summary += QStringLiteral(" native_request=%1").arg(id);
    if (!routine) {
        const auto statusName = status == EchoStatus::ApiFailure ? "api_failure"
                                : status == EchoStatus::Internal ? "internal"
                                : status == EchoStatus::Closed   ? "closed"
                                                                 : "invalid_input";
        summary += QStringLiteral(" status=%1").arg(QString::fromLatin1(statusName));
        const auto kind = error.value("kind").toString();
        if (!kind.isEmpty())
            summary += QStringLiteral(" kind=%1").arg(kind);
        if (code.isDouble())
            summary += QStringLiteral(" code=%1").arg(code.toInteger());
    }
    if (pending.retried)
        summary += QStringLiteral(" retried=true");
    const QMessageLogger logger(nullptr, 0, nullptr, category.constData());
    if (routine)
        logger.debug().noquote() << summary;
    else
        logger.warning().noquote() << summary;
    if (status == EchoStatus::ApiFailure && error.value("kind") == QStringLiteral("challenge") &&
        pending.automaticVerification && !pending.verifiesRequest) {
        if (pending.retried) {
            emit failed(original, status,
                        QByteArrayLiteral("{\"kind\":\"verification_retry_rejected\",\"message\":"
                                          "\"验证后原请求仍被拒绝，请稍后重试\"}"));
            return;
        }
        const auto eventId =
            error.value("challenge").toObject().value("event_id").toString().trimmed();
        if (!eventId.isEmpty()) {
            pending.eventId = eventId;
            m_suspended.insert(original, std::move(pending));
            m_verificationQueue.append(original);
            presentNextVerification();
            return; // Logical request has not reached a terminal state yet.
        }
    }
    if (status == EchoStatus::Success && pending.verifiesRequest) {
        const auto target = m_suspended.constFind(pending.verifiesRequest);
        if (target != m_suspended.cend() && error.value("acknowledged").toBool())
            retryVerifiedEvent(target->eventId);
    }
    if (m_closing || revision != m_revision)
        return; // A direct signal may switch accounts.
    if (status == EchoStatus::ApiFailure &&
        error.value("kind") == QStringLiteral("authentication") &&
        pending.request.value("identity").toObject().value("authentication") ==
            QStringLiteral("current_account") &&
        m_userId) {
        emit authenticationExpired();
        if (m_closing || revision != m_revision)
            return;
    }
    if (status == EchoStatus::Success)
        emit succeeded(original, payload);
    else
        emit failed(original, status, payload);
}

void ApiClient::presentNextVerification()
{
    if (m_activeVerification || m_closing)
        return;
    while (!m_verificationQueue.isEmpty()) {
        const auto id = m_verificationQueue.takeFirst();
        if (!m_suspended.contains(id))
            continue;
        m_activeVerification = id;
        emit verificationRequired(id, m_suspended.value(id).eventId);
        return;
    }
}

bool ApiClient::awaitLoginVerification(quint64 requestId)
{
    if (requestId != m_activeVerification || !m_suspended.contains(requestId) ||
        !m_loginConfirmation.isEmpty())
        return false;
    const auto event = m_suspended.value(requestId).eventId;
    for (const auto id : m_suspended.keys())
        if (m_suspended.value(id).eventId == event) {
            m_loginConfirmation.insert(id, m_suspended.take(id));
            m_verificationQueue.removeAll(id);
        }
    m_activeVerification = 0;
    emit verificationDismissed();
    presentNextVerification();
    return true;
}
void ApiClient::cancelLoginVerification()
{
    const auto confirmations = std::exchange(m_loginConfirmation, {});
    for (const auto &pending : confirmations)
        emit failed(pending.logicalId, EchoStatus::Cancelled,
                    QByteArrayLiteral("{\"kind\":\"cancelled\"}"));
}
quint64 ApiClient::verificationInfo(quint64 requestId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto original = m_suspended.constFind(requestId);
    if (requestId != m_activeVerification || original == m_suspended.cend())
        return 0;
    Pending pending;
    pending.revision = original->revision;
    pending.automaticVerification = false; // Verification cannot recursively prompt.
    pending.verifiesRequest = requestId;
    pending.request = {{"operation", "get_verify_info"},
                       {"identity", original->request.value("identity")},
                       {"query", QJsonObject{{"event_id", original->eventId}}}};
    return submitPrepared(std::move(pending));
}

quint64 ApiClient::submitVerification(quint64 requestId, quint32 verifyType, const QString &proof)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const auto original = m_suspended.constFind(requestId);
    if (requestId != m_activeVerification || original == m_suspended.cend() ||
        proof.trimmed().isEmpty() || (verifyType != 23 && verifyType != 32))
        return 0;
    // One submit in flight per active challenge, avoiding duplicate proof writes.
    for (const auto &pending : std::as_const(m_pending))
        if (pending.verifiesRequest == requestId)
            return 0;
    Pending pending;
    pending.revision = original->revision;
    pending.automaticVerification = false;
    pending.verifiesRequest = requestId;
    pending.request = {{"operation", "sidedt"},
                       {"identity", original->request.value("identity")},
                       {"query", QJsonObject{{"event_id", original->eventId},
                                             {"verify_type", qint64(verifyType)},
                                             {"proof", proof}}}};
    return submitPrepared(std::move(pending));
}

void ApiClient::retryVerifiedEvent(const QString &eventId)
{
    // Copy before erasing: eventId may refer to an element of m_suspended.
    const QString event = eventId;
    for (const auto id : m_suspended.keys()) {
        if (m_suspended.value(id).eventId != event)
            continue;
        auto pending = m_suspended.take(id);
        pending.retried = true;
        m_verificationQueue.removeAll(id);
        if (!pending.replayAfterVerification) {
            emit failed(id, EchoStatus::ApiFailure,
                        QByteArrayLiteral("{\"kind\":\"verification_complete\",\"message\":"
                                          "\"安全验证已完成，请确认后继续上传\"}"));
            continue;
        }
        if (!submitPrepared(std::move(pending)))
            emit failed(id, EchoStatus::Closed, QByteArrayLiteral("{\"kind\":\"closed\"}"));
    }
    m_activeVerification = 0;
    emit verificationDismissed();
    presentNextVerification();
}

void ApiClient::advanceSession()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++m_revision;
    for (auto id : m_pending.keys())
        echo_runtime_cancel(m_runtime, id);
    m_pending.clear();
    m_suspended.clear();
    m_loginConfirmation.clear();
    m_verificationQueue.clear();
    if (std::exchange(m_activeVerification, 0))
        emit verificationDismissed();
    emit sessionChanged(m_revision);
}

bool ApiClient::cancel(quint64 requestId)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (m_closing)
        return false;
    // Type-38 transfers ownership from the old page/QR service to the login
    // confirmation UI. Its teardown must not cancel the continuation; explicit
    // dismissal uses cancelLoginVerification(). Session changes still discard it.
    if (m_loginConfirmation.contains(requestId))
        return false;
    if (m_suspended.contains(requestId)) {
        const auto cancellationRevision = m_revision;
        const auto event = m_suspended.value(requestId).eventId;
        const bool dismissActive = m_suspended.value(m_activeVerification).eventId == event;
        QList<quint64> cancelled;
        for (const auto id : m_suspended.keys()) {
            if (m_suspended.value(id).eventId != event)
                continue;
            m_suspended.remove(id);
            m_verificationQueue.removeAll(id);
            cancelled.append(id);
        }
        for (const auto id : m_pending.keys()) {
            if (cancelled.contains(m_pending.value(id).verifiesRequest)) {
                echo_runtime_cancel(m_runtime, id);
                cancelled.append(m_pending.value(id).logicalId);
                m_pending.remove(id); // Late success cannot resume cancelled requests.
            }
        }
        if (dismissActive) {
            m_activeVerification = 0;
            emit verificationDismissed();
            presentNextVerification();
        }
        for (const auto id : cancelled) {
            if (m_closing || m_revision != cancellationRevision)
                break;
            emit failed(id, EchoStatus::Cancelled, QByteArrayLiteral("{\"kind\":\"cancelled\"}"));
        }
        return true;
    }
    for (auto it = m_pending.cbegin(); it != m_pending.cend(); ++it)
        if (it->logicalId == requestId) {
            const auto nativeId = it.key();
            const auto revision = it->revision;
            // Rust may already have completed while its GUI callback is queued.
            // The Qt logical request is still cancellable: invalidate it before
            // that callback runs, independently of the worker's cancellation result.
            m_pending.remove(nativeId);
            (void)echo_runtime_cancel(m_runtime, nativeId);
            QMetaObject::invokeMethod(
                this,
                [this, requestId, revision] {
                    if (!m_closing && revision == m_revision)
                        emit failed(requestId, EchoStatus::Cancelled,
                                    QByteArrayLiteral("{\"kind\":\"cancelled\"}"));
                },
                Qt::QueuedConnection);
            return true;
        }
    return false;
}

void ApiClient::shutdown()
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (std::exchange(m_closing, true))
        return;
    auto runtime = std::exchange(m_runtime, nullptr);
    m_pending.clear();
    m_suspended.clear();
    m_loginConfirmation.clear();
    m_verificationQueue.clear();
    if (std::exchange(m_activeVerification, 0))
        emit verificationDismissed();
    // Never wait for callbacks while the GUI event loop is blocked. The native
    // worker does not depend on GUI delivery, only on dispatcher lifetime.
    m_closer = QThread::create([runtime] { echo_runtime_destroy(runtime); });
    connect(m_closer, &QThread::finished, m_dispatcher, &QObject::deleteLater);
    connect(m_closer, &QThread::finished, this, &ApiClient::closed);
    connect(m_closer, &QThread::finished, m_closer, &QObject::deleteLater);
    m_closer->start();
}
} // namespace EchoMusic

quint64 EchoMusic::ApiClient::cloudSource(const QJsonObject &source)
{
    return submit({{"operation", "user_cloud_url"},
                   {"query", QJsonObject{{"hash", source.value("hash")},
                                         {"album_audio_id", source.value("song_id").toInteger()},
                                         {"audio_id", source.value("audio_id").toInteger()},
                                         {"name", source.value("name").toString()}}}},
                  true, {AuthenticationMode::CurrentAccount, true});
}
