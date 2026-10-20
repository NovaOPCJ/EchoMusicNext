#include "ArtistDetailService.h"
#include "DetailsService.h"
#include "AlbumDetailService.h"
#include <ctime>
#include <limits>
#include <QScopeGuard>
#include <utility>
#include "AccountUiFixture.h"
#include "NativeWindow.h"
#include "ProfileRouteCoordinator.h"
#include "SidebarRouteCoordinator.h"
#include "ProfileService.h"
#include "PlaylistOrderModel.h"
#include "PlaylistImportTaskCoordinator.h"
#include "PlaybackService.h"
#include "StartupController.h"
#include "StorageService.h"
#include "DeviceRegistrationService.h"
#include "SearchModel.h"
#include "SearchRouteCoordinator.h"
#include "SettingsCatalog.h"
#include "ShortcutService.h"
#include "AboutService.h"
#include "SettingsDataService.h"
#include "ExperimentalSettingsService.h"
#include "PluginRuntime.h"
#include "PluginManager.h"
#include "NetworkBootstrap.h"
#include "LogService.h"
#include "RelaunchService.h"
#include "ShellController.h"
#include "PlaylistDetailService.h"
#include "ShellSettings.h"
#include "ThemeStore.h"
#include "UiTypography.h"
#include "WindowController.h"
#include "StartupIntegration.h"
#include <QJsonArray>
#include <QMetaEnum>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QAbstractItemModelTester>
#include <QDir>
#include <QElapsedTimer>
#include <QInputMethodEvent>
#include <QFontDatabase>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlIncubator>
#include <QQmlProperty>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDataStream>
#include <QClipboard>
#include <QCursor>
#include <QProcess>
#include <atomic>
#include <chrono>
#include <QMimeData>
#include <iostream>
#include <memory>
#include <QUuid>
#include <QtTest>
using namespace EchoMusic;
#ifdef Q_OS_MACOS
QVariantMap nativeWindowAppearance(QWindow *window);
QVariantMap nativeSystemMediaInfo();
QVariantMap nativeFloatingAppearance(QWindow *window);
QVariantMap nativeMacComposition(QWindow *window, qint64 hostWindow);
#endif
namespace EchoMusic
{
// Domain-state seam for visual selected-state checks, not a provider response fixture.
class PlaybackLibraryActionsTestAccess
{
public:
    static void favorite(PlaybackLibraryActions &actions, const PlaybackTrack &track, bool value)
    {
        if (value)
            actions.m_favorites.insert(track.hash.toUpper(), 1);
        else
            actions.m_favorites.remove(track.hash.toUpper());
        emit actions.changed();
    }
};
class TrackTransitionControllerTestAccess
{
public:
    static bool prepared(const TrackTransitionController &controller)
    {
        return controller.m_prepared;
    }
};
class AccountServiceTestAccess
{
public:
    static QJsonObject membershipDomain(const AccountService &account)
    {
        return account.m_membership->domain();
    }
    static bool membershipKnown(const AccountService &account)
    {
        return account.m_membership->facts().known;
    }
    static QByteArray snapshot(const AccountService &account)
    {
        return account.storedSession();
    }
    static bool adopt(AccountService &account, const QByteArray &payload)
    {
        return account.adoptStoredSession(payload);
    }
    // Domain-only display/lifecycle seam, not a fabricated upstream sample.
    static quint64 profileRequest(AccountService &account, ApiClient &api)
    {
        return account.m_detail = api.capabilities();
    }
    static quint64 vipRequest(AccountService &account, ApiClient &api)
    {
        account.m_membership->m_revision = api.sessionRevision();
        return account.m_membership->m_request = api.capabilities();
    }
    static quint64 identityRequest(AccountService &account, ApiClient &api)
    {
        return account.m_identity = api.capabilities();
    }
    static quint64 gradeRequest(AccountService &account, ApiClient &api)
    {
        return account.m_gradeRequest = api.capabilities();
    }
    static void complete(AccountService &account, quint64 id, const QByteArray &payload)
    {
        if (id && id == account.m_membership->m_request)
            account.m_membership->succeed(id, payload);
        else
            account.succeed(id, payload);
    }
};
class PlaylistOrderModelTestAccess
{
public:
    static quint64 request(const PlaylistOrderModel &model)
    {
        return model.m_request;
    }
    static void complete(PlaylistOrderModel &model, quint64 id, const QByteArray &payload)
    {
        model.succeed(id, payload);
    }
    static QList<ApiClient::PlaylistPosition> positions(const PlaylistOrderModel &model)
    {
        return model.positions();
    }
};
class PlaybackServiceTestAccess
{
public:
    static void sourcePresentation(PlaybackService &p, const QString &kind)
    {
        quality(p, "standard");
        p.m_sourceKind = kind;
        p.m_loading = false;
        p.m_privileges = {{"variants", QJsonArray{QJsonObject{{"quality", "128"},
                                                              {"hash", p.m_track.hash},
                                                              {"privilege", 0},
                                                              {"service_status", 1}}}}};
        p.m_track.catalogMetadata = {{"cloud_source", QJsonObject{{"hash", p.m_track.hash},
                                                                  {"hash_std", p.m_track.hash},
                                                                  {"size_bytes", 23456789}}}};
        emit p.changed();
    }
    // Actual public lyric API with presentation metadata, independent of audio URL authorization.
    static void lyricMetadata(PlaybackService &playback, const PlaybackTrack &track)
    {
        playback.m_track = track;
        playback.m_duration = qint64(track.durationMs);
        playback.m_lyricSource.load(track);
        emit playback.changed();
    }
    // Presentation-only domain state; live source authorization is tested separately.
    static void quality(PlaybackService &playback, const QString &resolved)
    {
        playback.m_track = {};
        playback.m_track.id = "quality-presentation";
        playback.m_track.hash = "105A2705BEEE36A8BED029A8E1A6311D";
        playback.m_track.title = "Quality presentation";
        playback.m_quality = "viper_tape";
        playback.m_resolvedQuality = resolved;
        emit playback.changed();
    }
};
class PlaylistImportServiceTestAccess
{
public:
    // Canonical pending business state, not a fabricated upstream response.
    static void awaitDuplicate(PlaylistImportService &service, const QString &title)
    {
        service.m_phase = PlaylistImportService::Preparing;
        service.m_stage = PlaylistImportService::None;
        service.m_duplicate = title;
        emit service.changed();
        emit service.duplicateConfirmationRequested();
    }
    static void pausePoll(PlaylistImportService &service)
    {
        service.m_poll.stop();
    }
    static void poll(PlaylistImportService &service)
    {
        service.poll();
    }
    static quint64 target(const PlaylistImportService &service)
    {
        return service.m_target;
    }
    static quint64 request(const PlaylistImportService &service)
    {
        return service.m_request;
    }
    static void complete(PlaylistImportService &service, const QByteArray &payload)
    {
        const auto request = service.m_request;
        service.m_api.cancel(request);
        service.succeed(request, payload);
    }
    static void stale(PlaylistImportService &service, quint64 request, const QByteArray &payload)
    {
        service.succeed(request, payload);
    }
    static void prepareScreenshot(PlaylistImportService &service, quint64 target,
                                  const QString &title)
    {
        QVERIFY(service.beginRun());
        service.m_target = target;
        service.m_title = title;
        service.m_images = {QByteArray("domain-image")};
        service.beginCatalog(target == 0);
    }
};
class UserPlaylistModelTestAccess
{
public:
    static void complete(UserPlaylistModel &model, ApiClient &api, const QByteArray &payload)
    {
        model.m_sessionRevision = api.sessionRevision();
        model.m_request = api.capabilities();
        const auto id = model.m_request;
        api.cancel(id);
        model.succeed(id, payload);
    }
    static void present(UserPlaylistModel &model, ApiClient &api, int count)
    {
        if (model.m_request)
            api.cancel(model.m_request);
        model.m_request = 0;
        model.beginResetModel();
        model.m_rows.clear();
        for (int i = 0; i < count; ++i) {
            UserPlaylistModel::Row row;
            row.cloudId = {quint64(i + 1)};
            row.globalId = QString("display-playlist-%1").arg(i + 1);
            row.creatorGlobalId = row.globalId;
            row.creatorId = 900001;
            row.title = i == 0   ? QStringLiteral("默认收藏")
                        : i == 1 ? QStringLiteral("我喜欢")
                                 : QString("领域歌单 %1").arg(i + 1);
            row.sourceCode = 1;
            row.sortIndex = i;
            row.editable = true;
            if (i < 2)
                row.defaultKind = i + 1;
            model.m_rows.append(row);
        }
        model.m_sessionRevision = api.sessionRevision();
        model.m_total = count;
        model.m_exhausted = true;
        model.m_state = UserPlaylistModel::State::Ready;
        model.endResetModel();
        emit model.changed();
    }
    static void collectSecond(UserPlaylistModel &model)
    {
        auto &row = model.m_rows[1];
        row.kind = QStringLiteral("collected");
        row.collected = true;
        row.globalId = QStringLiteral("history-collected-local");
        row.creatorGlobalId = QStringLiteral("history-collected-public");
        row.title = QStringLiteral("收藏联动歌单");
        row.defaultKind.reset();
        row.editable = false;
        emit model.dataChanged(model.index(1), model.index(1), {});
        emit model.changed();
    }
};
class SavedAccountModelTestAccess
{
public:
    static void present(SavedAccountModel &model, const StoredAccounts &rows,
                        const QByteArray &session)
    {
        // Isolate view geometry from persistence. Do not enqueue a persistent account
        // write when the account display fixture announces its session.
        Q_ASSERT(!model.m_ready);
        model.m_queue.clear();
        model.m_lastSession = session;
        model.publish(rows);
    }
};
class PlaylistMutationServiceTestAccess
{
public:
    static quint64 request(PlaylistMutationService &s)
    {
        return s.m_request;
    }
    static void reject(PlaylistMutationService &s)
    {
        s.fail(s.m_request, R"({"kind":"business"})");
    }
};
class PlaylistDetailContextTestAccess
{
public:
    static void seed(PlaylistDetailContext &c)
    {
        c.m_detail = {
            {"global_id", "public-playlist"}, {"title", "歌单收藏测试"},   {"creator_id", 42},
            {"creator_list_id", 287},         {"creator_name", "Creator"}, {"source_code", 1}};
        c.m_complete = true;
        emit c.changed();
    }
};
class AlbumDetailContextTestAccess
{
public:
    static void seed(AlbumDetailContext &c)
    {
        c.m_detail = {{"id", 960399},
                      {"title", "专辑收藏测试"},
                      {"artists", QJsonArray{QJsonObject{{"id", 3520}, {"name", "周杰伦"}}}}};
        c.m_complete = true;
        emit c.changed();
    }
};
class ArtistDetailContextTestAccess
{
public:
    static void seed(ArtistDetailContext &c)
    {
        c.m_detail = {{"id", 3060},
                      {"name", "歌手关注测试"},
                      {"fans", 12450},
                      {"description", "Artist description"}};
        c.m_complete = true;
        emit c.changed();
    }
};
class ProfileServiceTestAccess
{
public:
    static void seed(ProfileService &p, int tab, QJsonArray rows)
    {
        const auto old = std::exchange(p.m_social[tab].request, 0);
        if (old)
            p.m_api.cancel(old);
        p.m_social[tab].loaded = true;
        p.m_social[tab].model->replace(rows);
        emit p.changed();
    }
    static quint64 followWrite(ProfileService &p)
    {
        return p.m_followWrites.isEmpty() ? 0 : p.m_followWrites.keys().first();
    }
    static quint64 chatRead(ProfileService &p)
    {
        return p.m_chatRead;
    }
    static quint64 chatWrite(ProfileService &p)
    {
        return p.m_chatWrite;
    }
    // Feed the already-validated canonical bridge contract, without network
    // writes or inventing an upstream response fixture.
    static quint64 socialRequest(ProfileService &service, ApiClient &api, int tab)
    {
        return service.m_social[tab].request = api.capabilities();
    }
    static quint64 deviceRequest(ProfileService &service, ApiClient &api)
    {
        return service.m_deviceRequest = api.capabilities();
    }
    static quint64 deviceKick(const ProfileService &service)
    {
        return service.m_deviceKick;
    }
    static quint64 deviceRead(const ProfileService &service)
    {
        return service.m_deviceRequest;
    }
    static quint64 saveRequest(ProfileService &service, ApiClient &api,
                               const ApiClient::ProfilePatch &patch)
    {
        service.m_patch = patch;
        return service.m_save = api.capabilities();
    }
    static quint64 avatarRequest(ProfileService &service, ApiClient &api)
    {
        return service.m_avatarRequest = api.capabilities();
    }
    static void complete(ProfileService &service, quint64 id, const QByteArray &payload)
    {
        service.succeeded(id, payload);
    }
    static void fail(ProfileService &service, quint64 id, const QByteArray &payload)
    {
        service.failed(id, payload);
    }
};
class ContentBlacklistTestAccess
{
public:
    static quint64 singerWrite(const ContentBlacklistService &service)
    {
        return service.m_singerWrite;
    }
    static quint64 read(ContentBlacklistService &service, ApiClient &api, int category,
                        int page = 1)
    {
        auto &b = service.m_buckets[category];
        if (b.request)
            api.cancel(std::exchange(b.request, 0));
        b.requestedPage = page;
        return b.request = api.capabilities();
    }
    static quint64 hold(ContentBlacklistService &service, ApiClient &api, int category)
    {
        auto &b = service.m_buckets[category];
        if (b.request)
            api.cancel(b.request);
        b.requestedPage = 1;
        b.error.clear();
        b.request = std::numeric_limits<quint64>::max() - category;
        emit service.changed();
        return b.request;
    }
    static quint64 request(const ContentBlacklistService &service, int category)
    {
        return service.m_buckets[category].request;
    }
    static quint64 write(const ContentBlacklistService &service)
    {
        return service.m_write;
    }
    static void complete(ContentBlacklistService &service, ApiClient &api, quint64 id,
                         const QByteArray &bytes)
    {
        api.cancel(id);
        service.succeeded(id, bytes);
    }
    static void fail(ContentBlacklistService &service, ApiClient &api, quint64 id)
    {
        api.cancel(id);
        service.failed(id, R"({"message":"呈现回归中的请求失败"})");
    }
    static quint64 songIndexRequest(const ContentBlacklistService &service)
    {
        return service.m_songIndex.request;
    }
    static quint64 menuWrite(const ContentBlacklistService &service)
    {
        return service.m_menuWrite;
    }
};
class ApiClientTestAccess
{
public:
    static QJsonObject request(const ApiClient &api, quint64 id)
    {
        return api.m_pending.value(id).request;
    }
    static bool automaticVerification(const ApiClient &api, quint64 id)
    {
        return api.m_pending.value(id).automaticVerification;
    }
};
class ListeningPreferencesTestAccess
{
public:
    static quint64 read(ListeningPreferencesService &service, ApiClient &api)
    {
        return service.m_read = api.capabilities();
    }
    static void complete(ListeningPreferencesService &service, quint64 id, const QByteArray &bytes)
    {
        service.succeeded(id, bytes);
    }
    static quint64 submitted(ListeningPreferencesService &service, ApiClient &api)
    {
        service.m_submitted = service.m_draft;
        return service.m_save = api.capabilities();
    }
    static void fail(ListeningPreferencesService &service, quint64 id)
    {
        service.failed(id);
    }
    static ApiClient::PreferencePatch patch(const ListeningPreferencesService &service)
    {
        return ListeningPreferencesService::changes(service.m_baseline, service.m_draft);
    }
};
} // namespace EchoMusic
namespace
{
QQuickItem *visual(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *found = visual(child, name))
            return found;
    return nullptr;
}
QList<QQuickItem *> visualAll(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> result;
    if (root->objectName() == name)
        result.append(root);
    for (auto *child : root->childItems())
        result.append(visualAll(child, name));
    return result;
}
QQuickItem *popupScrollBarFor(QQuickWindow *window, QQuickItem *view)
{
    for (auto *bar : visualAll(window->contentItem(), "popupScrollBar"))
        if (bar->property("view").value<QQuickItem *>() == view)
            return bar;
    return nullptr;
}
void verifyPopupScrollMapping(QQuickWindow *window, QQuickItem *view)
{
    auto *bar = popupScrollBarFor(window, view);
    QVERIFY(bar);
    QTRY_COMPARE(bar->property("orientation").toInt(), int(Qt::Vertical));
    QTRY_VERIFY(qAbs(bar->property("size").toReal() -
                     QQmlProperty(view, "visibleArea.heightRatio").read().toReal()) < 0.001);
    QTRY_VERIFY(qAbs(bar->property("position").toReal() -
                     QQmlProperty(view, "visibleArea.yPosition").read().toReal()) < 0.001);
    QTRY_VERIFY(view->property("contentHeight").toReal() > view->height() + 10);
    view->setProperty("contentY", view->property("originY"));
    QTRY_VERIFY(bar->property("position").toReal() < 0.001);
    // Qt's attached bar must track both scrolling the content and dragging the
    // reparented visual thumb. Geometry alone does not establish this connection.
    view->setProperty("contentY", view->property("originY").toReal() + 30);
    QTRY_VERIFY(bar->property("position").toReal() > 0);
    QTRY_VERIFY(qAbs(bar->property("position").toReal() -
                     QQmlProperty(view, "visibleArea.yPosition").read().toReal()) < 0.001);
    view->setProperty("contentY", view->property("originY"));
    QTRY_VERIFY(bar->property("position").toReal() < 0.001);
    auto *thumb = visual(bar, "scrollThumb");
    QVERIFY(thumb);
    QVERIFY(Test::prepareNativeInput(window));
    QTest::qWait(40);
    const auto from = thumb->mapToScene({thumb->width() / 2, thumb->height() / 2}).toPoint();
    const auto to = bar->mapToScene({bar->width() / 2, bar->height() - 10}).toPoint();
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(window, to, 20);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
    QTRY_VERIFY(view->property("contentY").toReal() > view->property("originY").toReal() + 10);
    QTRY_VERIFY(qAbs(bar->property("position").toReal() -
                     QQmlProperty(view, "visibleArea.yPosition").read().toReal()) < 0.001);
    view->setProperty("contentY", view->property("originY"));
}
QQuickItem *visualText(QQuickItem *root, const QString &text)
{
    if (root->isVisible() && root->property("text").toString() == text &&
        root->property("color").isValid())
        return root;
    for (auto *child : root->childItems())
        if (auto *found = visualText(child, text))
            return found;
    return nullptr;
}
qreal glyphCenterY(QQuickWindow *window, const QImage &image, QQuickItem *row)
{
    auto *text = row->property("textItem").value<QQuickItem *>();
    if (!text)
        text = row;
    const qreal ratio = image.width() / qreal(window->width());
    const auto bounds = text->mapRectToScene(QRectF(0, 0, text->width(), text->height()));
    const auto pixels = QRectF(bounds.topLeft() * ratio, bounds.size() * ratio)
                            .toAlignedRect()
                            .intersected(image.rect());
    QHash<QRgb, int> frequencies;
    for (int y = pixels.top(); y <= pixels.bottom(); ++y)
        for (int x = pixels.left(); x <= pixels.right(); ++x)
            ++frequencies[image.pixel(x, y)];
    QRgb background = 0;
    int count = 0;
    for (auto it = frequencies.cbegin(); it != frequencies.cend(); ++it)
        if (it.value() > count) {
            background = it.key();
            count = it.value();
        }
    int top = image.height(), bottom = -1;
    for (int y = pixels.top(); y <= pixels.bottom(); ++y)
        for (int x = pixels.left(); x <= pixels.right(); ++x) {
            const auto pixel = image.pixel(x, y);
            if (std::max({qAbs(qRed(pixel) - qRed(background)),
                          qAbs(qGreen(pixel) - qGreen(background)),
                          qAbs(qBlue(pixel) - qBlue(background))}) > 25) {
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    return bottom >= top ? (top + bottom + 1) / (2 * ratio) : qQNaN();
}
class FloatingFrameProbe final : public QObject
{
    struct Counters
    {
        std::atomic<qint64> begin{0}, renderBegin{0}, peak{0}, syncPeak{0};
        std::atomic<quint64> frames{0};
    };

public:
    explicit FloatingFrameProbe(QQuickWindow *window) : QObject(window)
    {
        const auto counters = m_counters;
        connect(
            window, &QQuickWindow::beforeSynchronizing, this,
            [counters] { counters->begin.store(now()); }, Qt::DirectConnection);
        connect(
            window, &QQuickWindow::afterSynchronizing, this,
            [counters] {
                const auto begin = counters->begin.exchange(0);
                if (!begin)
                    return;
                const auto span = now() - begin;
                auto peak = counters->syncPeak.load();
                while (span > peak && !counters->syncPeak.compare_exchange_weak(peak, span)) {
                }
            },
            Qt::DirectConnection);
        connect(
            window, &QQuickWindow::beforeRendering, this,
            [counters] { counters->renderBegin.store(now()); }, Qt::DirectConnection);
        connect(
            window, &QQuickWindow::afterRendering, this,
            [counters] {
                // Separate GUI synchronization waits from render recording,
                // and consume each phase once when a scene is reused.
                const auto begin = counters->renderBegin.exchange(0);
                if (!begin)
                    return;
                const auto span = now() - begin;
                ++counters->frames;
                auto peak = counters->peak.load();
                while (span > peak && !counters->peak.compare_exchange_weak(peak, span)) {
                }
            },
            Qt::DirectConnection);
    }
    quint64 frames() const
    {
        return m_counters->frames.load();
    }
    double maximumMs() const
    {
        return m_counters->peak.load() / 1000.;
    }
    double maximumSyncMs() const
    {
        return m_counters->syncPeak.load() / 1000.;
    }

private:
    static qint64 now()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
    const std::shared_ptr<Counters> m_counters = std::make_shared<Counters>();
};
qint64 floatingRssKiB()
{
#ifdef Q_OS_UNIX
    QProcess process;
    process.start("ps", {"-o", "rss=", "-p", QString::number(QCoreApplication::applicationPid())});
    if (process.waitForFinished(1000))
        return process.readAllStandardOutput().trimmed().toLongLong();
#endif
    return -1;
}
bool writeFloatingTestWave(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    constexpr quint32 bytes = 48000 * 16 * 2;
    stream.writeRawData("RIFF", 4);
    stream << quint32(36 + bytes);
    stream.writeRawData("WAVEfmt ", 8);
    stream << quint32(16) << quint16(1) << quint16(1) << quint32(48000) << quint32(96000)
           << quint16(2) << quint16(16);
    stream.writeRawData("data", 4);
    stream << bytes;
    for (int i = 0; i < 48000 * 16; ++i)
        stream << qint16(std::sin(i * 2. * 3.141592653589793 * 220 / 48000) * 800);
    return stream.status() == QDataStream::Ok;
}
struct Session
{
    QTemporaryDir directory;
    QSettings preferences{directory.filePath("preferences.ini"), QSettings::IniFormat};
    ThemeStore theme{preferences};
    ShellSettings shell{preferences};
    WindowController windows{preferences, false};
    ApiClient api{DeviceIdentity::temporary()};
    Test::AccountUiFixture accounts{api};
    ProfileService profile{api, accounts.account};
    SearchModel search{api};
    RouteController router;
    SidebarRouteCoordinator sidebarRoutes{accounts.library, accounts.playlists, router};
    ShellController commands{preferences, router};
    SearchRouteCoordinator searchRoutes{search, router, false};
    ProfileRouteCoordinator profileRoutes{accounts.controller, profile, router, false};
    QQmlEngine engine;
    QScopedPointer<QObject> root;
    QQuickWindow *window = nullptr;
    QString initialRoute = QStringLiteral("profile");
    bool create(PlaybackService *playback = nullptr, NetworkPreferences *network = nullptr,
                LogService *logging = nullptr, RelaunchService *relaunch = nullptr,
                StartupController *startup = nullptr,
                ExperimentalSettingsService *experimental = nullptr)
    {
        if (playback) {
            playback->attachLibrary(accounts.playlists, accounts.mutations);
            commands.attachPlayback(*playback);
            commands.attachWindows(windows);
            playback->songCommands()->attach(*profile.blacklist(), accounts.account);
        }
        router.navigate(initialRoute);
        theme.attachToEngine(engine);
        QQmlComponent component(&engine, QUrl("qrc:/qt/qml/EchoMusic/App/Main.qml"));
        root.reset(component.createWithInitialProperties(
            {{"experimentalSettingsService", QVariant::fromValue(experimental)},
             {"startup", QVariant::fromValue(startup)},
             {"playbackService", QVariant::fromValue(playback)},
             {"networkPreferences", QVariant::fromValue(network)},
             {"logService", QVariant::fromValue(logging)},
             {"relaunchService", QVariant::fromValue(relaunch)},
             {"libraryController", QVariant::fromValue(&accounts.library)},
             {"accountController", QVariant::fromValue(&accounts.controller)},
             {"shellController", QVariant::fromValue(&commands)},
             {"searchService", QVariant::fromValue(&search)},
             {"router", QVariant::fromValue(&router)},
             {"shellSettings", QVariant::fromValue(&shell)},
             {"windowService", QVariant::fromValue(&windows)}}));
        if (!root) {
            qWarning().noquote() << component.errorString();
            return false;
        }
        window = qobject_cast<QQuickWindow *>(root.data());
        windows.attach(window);
        return window;
    }
    ~Session()
    {
        router.shutdown();
        windows.shutdown();
        search.shutdown();
        api.shutdown();
        if (window)
            window->close();
        root.reset();
    }
};
QByteArray account(quint64 id)
{
    return QString("{\"user_id\":%1,\"token\":\"offline-display-fixture\",\"nickname\":\"Qt "
                   "测试账号\",\"avatar_url\":\"\"}")
        .arg(id)
        .toUtf8();
}
QByteArray socialRows(int count)
{
    QJsonArray users;
    for (int i = 0; i < count; ++i)
        users.append(QJsonObject{
            {"key", QString("user:%1").arg(i + 1)},
            {"user_id", i + 1},
            {"artist_id", 0},
            {"nickname", QString("领域列表用户 %1").arg(i + 1)},
            {"avatar_url", ""},
            {"description", "领域展示压力测试"},
            {"following", true},
            {"mutual", false},
            {"identity",
             i == 0 ? QJsonObject{{"talent_bits", 3}, {"singer", false}} : QJsonObject{}},
            {"membership", i == 0 ? QJsonObject{{"deluxe_active", true}} : QJsonObject{}}});
    return QJsonDocument(QJsonObject{{"users", users}, {"total", count}}).toJson();
}
QByteArray preferencesDomain()
{
    const auto weight = [](QJsonObject values) {
        return QJsonObject{
            {"encoded", QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact))},
            {"weights", values}};
    };
    return QJsonDocument(QJsonObject{{"recommendation_mode", ""},
                                     {"gender", "other-version"},
                                     {"age", ""},
                                     {"recommendation", weight({{"125", 100}})},
                                     {"languages", weight({{"2", 100}})},
                                     {"styles", weight({})}})
        .toJson();
}
int visualCount(QQuickItem *root, const QString &name)
{
    int count = root->objectName() == name ? 1 : 0;
    for (auto *child : root->childItems())
        count += visualCount(child, name);
    return count;
}
} // namespace
class DesktopPagesTest : public QObject
{
    Q_OBJECT
private slots:
    void startupUsesRealFailuresAndReleasesItsEntirePage()
    {
        const auto artifacts = qEnvironmentVariable("ECHO_STARTUP_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(QDir().mkpath(artifacts));
        for (const bool deviceFailure : {false, true}) {
            Session session;
            if (!deviceFailure) {
                auto identity = session.api.deviceIdentitySnapshot();
                identity.dfid = "stored-device-state-fixture";
                QVERIFY(session.api.setDeviceIdentity(identity));
            }
            NetworkPreferences network(session.preferences, session.api);
            if (deviceFailure) {
                network.apply("fixed_servers", {}, "http://127.0.0.1:1", {}, {}, false, {}, 1, 1);
                QTRY_VERIFY(!network.busy());
                bool applied = false;
                network.afterPendingMediaWork([&] { applied = true; });
                QTRY_VERIFY(applied);
            }
            PlaybackService playback(session.api, session.preferences, nullptr,
                                     session.directory.filePath("playback.sqlite"), &network);
            StorageService storage;
            DeviceRegistrationService device(session.api, session.preferences);
            const auto blockerPath = session.directory.filePath("blocked");
            QFile blocker(blockerPath);
            if (!deviceFailure) {
                QVERIFY(blocker.open(QIODevice::WriteOnly));
                blocker.close();
            }
            StartupController startup(storage, session.api, device, playback, network,
                                      session.accounts.saved,
                                      deviceFailure ? session.directory.filePath("music.sqlite")
                                                    : blockerPath + "/music.sqlite");
            QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
            QVERIFY(session.create(&playback, &network, nullptr, nullptr, &startup));
            const auto cleanup = qScopeGuard([&session] {
                session.root.reset();
                session.window = nullptr;
            });
            session.window->resize(1150, 750);
            QVERIFY(Test::prepareNativeInput(session.window));
            QPointer<QQuickItem> page = visual(session.window->contentItem(), "startupPage");
            QVERIFY(page);
            QVERIFY(!visual(session.window->contentItem(), "shellFrame"));
            auto *glow = visual(page, "startupGlowLoader");
            QVERIFY(glow);
            QTRY_VERIFY(glow->property("item").value<QObject *>());
            session.window->hide();
            QTRY_VERIFY(!glow->property("item").value<QObject *>());
            session.window->show();
            QVERIFY(Test::prepareNativeInput(session.window));
            if (!artifacts.isEmpty())
                QVERIFY(session.window->grabWindow().save(artifacts + "/loading.png"));
            startup.start();
            QTRY_VERIFY_WITH_TIMEOUT(startup.failed(), 15000);
            QCOMPARE(startup.canForceEnter(), deviceFailure);
            auto *force = visual(page, "startupForceEnter");
            auto *retry = visual(page, "startupRetry");
            auto *exit = visual(page, "startupExit");
            QVERIFY(force && retry && exit);
            QCOMPARE(force->isVisible(), deviceFailure);
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                session.theme.setMode(mode);
                for (int zoom : {100, 125}) {
                    session.shell.resetZoom();
                    for (int i = 100; i < zoom; i += 5)
                        session.shell.zoomIn();
                    QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                    session.window->update();
                    QTRY_VERIFY(!frames.isEmpty());
                    // A nested text maximum must not left-align the entire
                    // error group against the wider centered logo column.
                    auto *mark = visual(page, "startupMark");
                    QVERIFY(mark);
                    const auto centerX = mark->mapRectToScene(mark->boundingRect()).center().x();
                    QVERIFY(qAbs(centerX - session.window->width() / 2.) < 0.5);
                    for (const auto *name :
                         {"startupErrorGroup", "startupErrorIcon", "startupErrorHeading",
                          "startupErrorMessage", "startupErrorActions"}) {
                        auto *item = visual(page, name);
                        QVERIFY(item);
                        const auto actual = item->mapRectToScene(item->boundingRect()).center().x();
                        QVERIFY2(qAbs(actual - centerX) < 0.5, name);
                    }
                    const auto retryBounds = retry->mapRectToScene(retry->boundingRect());
                    QVERIFY(retryBounds.left() >= 0 &&
                            retryBounds.right() <= session.window->width());
                    if (!artifacts.isEmpty())
                        QVERIFY(session.window->grabWindow().save(
                            artifacts +
                            QString("/%1-%2-%3.png")
                                .arg(deviceFailure ? "device-failure" : "storage-failure")
                                .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                .arg(zoom)));
                }
            }
            auto *action = deviceFailure ? force : retry;
            if (!deviceFailure)
                QVERIFY(blocker.remove());
            const auto point =
                action->mapToScene({action->width() / 2, action->height() / 2}).toPoint();
            Test::moveNativePointer(session.window, point);
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            QTRY_VERIFY_WITH_TIMEOUT(startup.entered(), 15000);
            QTRY_VERIFY(page.isNull());
            QTRY_VERIFY(visual(session.window->contentItem(), "shellFrame"));
            startup.shutdown();
            QVERIFY(startup.entered());
            QVERIFY(!visual(session.window->contentItem(), "startupPage"));
            QCOMPARE(warnings.size(), 0);
            QSignalSpy storageClosed(&storage, &StorageService::closed);
            storage.shutdown();
            QTRY_COMPARE(storageClosed.size(), 1);
            device.shutdown();
            playback.shutdown();
        }
    }
    void recoveryStartupWindowRetriesAndExitsWithoutBusinessOwners()
    {
        QTemporaryDir directory;
        QSettings preferences(directory.filePath("settings.ini"), QSettings::IniFormat);
        ThemeStore theme(preferences);
        ShellSettings shell(preferences);
        WindowController windows(preferences, false);
        StartupController startup(QStringLiteral("restore transaction blocked"));
        QQmlEngine engine;
        theme.attachToEngine(engine);
        QQmlComponent component(&engine, QUrl("qrc:/qt/qml/EchoMusic/App/StartupWindow.qml"));
        QScopedPointer<QObject> root(component.createWithInitialProperties(
            {{"startup", QVariant::fromValue(&startup)},
             {"windowService", QVariant::fromValue(&windows)},
             {"shellSettings", QVariant::fromValue(&shell)}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        windows.attach(window);
        QVERIFY(Test::prepareNativeInput(window));
        startup.start();
        QTRY_VERIFY(startup.failed());
        QSignalSpy retries(&startup, &StartupController::restartApplicationRequested);
        auto *retry = visual(window->contentItem(), "startupRetry");
        QVERIFY(retry);
        retry->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Space);
        QCOMPARE(retries.size(), 1);
        startup.reportRestartFailure(QStringLiteral("helper launch failed"));
        QVERIFY(startup.failed());
        QSignalSpy quit(&startup, &StartupController::quitRequested);
        auto *exit = visual(window->contentItem(), "startupExit");
        QVERIFY(exit);
        exit->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Space);
        QCOMPARE(quit.size(), 1);
        startup.shutdown();
        windows.shutdown();
        window->close();
    }
    void settingsSearchClearRestoresCategoriesAndKeepsFocus()
    {
        Session session;
        NetworkPreferences network(session.preferences, session.api);
        QVERIFY(session.create(nullptr, &network));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.theme.setMotionEnabled(false);
        session.shell.setMotionEnabled(false);
        session.window->resize(1250, 900);
        const auto artifacts = qEnvironmentVariable("ECHO_TEXT_FIELD_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(QDir().mkpath(artifacts));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const int zoom : {100, 125}) {
                session.shell.resetZoom();
                for (int step = 100; step < zoom; step += 5)
                    session.shell.zoomIn();
                QVERIFY(Test::prepareNativeInput(session.window));
                session.router.navigate("settings");
                QPointer<QObject> dialog = session.root->findChild<QObject *>("settingsDialog");
                QVERIFY(dialog);
                QTRY_VERIFY(dialog->property("opened").toBool());
                auto *catalog = dialog->property("catalog").value<SettingsCatalog *>();
                QVERIFY(catalog);
                const auto fullCount = catalog->rowCount();
                QVERIFY(fullCount > 1);
                auto *trigger = visual(session.window->contentItem(), "settingsSearchTrigger");
                QVERIFY(trigger);
                trigger->forceActiveFocus(Qt::TabFocusReason);
                QTest::keyClick(session.window, Qt::Key_Space);
                auto *input = visual(session.window->contentItem(), "settingsSearchInput");
                QVERIFY(input);
                QTRY_VERIFY(input->isVisible() && input->hasActiveFocus());
                auto *clear = visual(input, "clearTextAction");
                QVERIFY(clear);
                QVERIFY(!clear->isVisible());
                for (const auto key : QByteArray("bypass"))
                    QTest::keyClick(session.window, key);
                QTRY_COMPARE(catalog->filter(), QString("bypass"));
                QCOMPARE(catalog->rowCount(), 1);
                QTRY_VERIFY(clear->isVisible());
                QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                session.window->update();
                QTRY_VERIFY(!frames.isEmpty());
                const auto capture = [&](const QString &phase) {
                    return artifacts.isEmpty() ||
                           session.window->grabWindow().save(
                               artifacts + QString("/settings-search-%1-%2-%3.png")
                                               .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                               .arg(zoom)
                                               .arg(phase));
                };
                QVERIFY(capture("filtered"));
                const auto point =
                    clear->mapToScene({clear->width() / 2, clear->height() / 2}).toPoint();
                Test::moveNativePointer(session.window, point);
                QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
                QTRY_COMPARE(catalog->filter(), QString());
                QCOMPARE(catalog->rowCount(), fullCount);
                QCOMPARE(input->property("text").toString(), QString());
                QVERIFY(input->isVisible() && input->hasActiveFocus());
                QVERIFY(!clear->isVisible());
                if (!artifacts.isEmpty()) {
                    frames.clear();
                    session.window->update();
                    QTRY_VERIFY(!frames.isEmpty());
                    QVERIFY(capture("cleared"));
                }
                for (const auto key : QByteArray("font"))
                    QTest::keyClick(session.window, key);
                QTRY_COMPARE(catalog->filter(), QString("font"));
                QVERIFY(catalog->rowCount() < fullCount);
                QTest::keyClick(session.window, Qt::Key_Escape);
                QTRY_COMPARE(catalog->filter(), QString());
                QTRY_VERIFY(!input->isVisible());
                QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
                QTRY_VERIFY(dialog.isNull());
            }
        }
    }
    void playbackFallbackControlsUseProductionUi()
    {
        Session session;
        session.initialRoute = "search";
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.router.shutdown();
            session.commands.shutdown();
            session.windows.shutdown();
            session.root.reset();
            session.window = nullptr;
        });
        session.theme.setMotionEnabled(false);
        session.shell.setMotionEnabled(false);
        session.window->resize(1250, 900);
        auto *settings = session.root->findChild<QObject *>("settingsDialogOwner");
        auto *quality = session.root->findChild<QObject *>("playbackOptionsPopupOwner");
        QVERIFY(settings && quality);
        QVERIFY(!settings->property("item").value<QObject *>());
        QVERIFY(!quality->property("item").value<QObject *>());
        const auto captures = qEnvironmentVariable("ECHO_SOURCE_UI_CAPTURES");
        if (!captures.isEmpty())
            QVERIFY(QDir().mkpath(captures));
        QVERIFY(!playback.preferences()->mvAudioFallbackEnabled());
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const int zoom : {100, 125}) {
                session.shell.resetZoom();
                for (int z = 100; z < zoom; z += 5)
                    session.shell.zoomIn();
                QVERIFY(QMetaObject::invokeMethod(
                    settings, "open",
                    Q_ARG(QVariant, (QVariantMap{{"initialSection", "playback"}}))));
                QPointer<QObject> dialog = settings->property("item").value<QObject *>();
                QVERIFY(dialog);
                QTRY_VERIFY(dialog->property("opened").toBool());
                auto *sectionLoader =
                    visual(session.window->contentItem(), "settingsSectionLoader");
                QVERIFY(sectionLoader);
                QTRY_COMPARE_WITH_TIMEOUT(sectionLoader->property("status").toInt(), 1, 15000);
                QQuickItem *toggle = nullptr;
                QTRY_VERIFY(
                    (toggle = visual(session.window->contentItem(), "settingsMvAudioFallback")));
                QTRY_VERIFY(toggle->width() > 0 && toggle->isVisible());
                auto *viewport = visual(session.window->contentItem(), "settingsContentViewport");
                QVERIFY(viewport);
                QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                session.window->update();
                QTRY_VERIFY(!frames.isEmpty());
                QCOMPARE(toggle->property("checked").toBool(), false);
                QVERIFY(Test::prepareNativeInput(session.window));
                QTRY_VERIFY([&] {
                    const auto local = toggle->mapToItem(viewport, {0, 0});
                    const auto maximum =
                        qMax(0., viewport->property("contentHeight").toReal() - viewport->height());
                    viewport->setProperty(
                        "contentY", qBound(0.,
                                           local.y() + viewport->property("contentY").toReal() -
                                               viewport->height() / 2,
                                           maximum));
                    const auto current = toggle->mapToItem(viewport, {0, 0});
                    return current.y() >= 0 && current.y() + toggle->height() <= viewport->height();
                }());
                QTest::mouseClick(
                    session.window, Qt::LeftButton, Qt::NoModifier,
                    toggle->mapToScene({toggle->width() / 2, toggle->height() / 2}).toPoint());
                QTRY_VERIFY(playback.preferences()->mvAudioFallbackEnabled());
                QCOMPARE(session.preferences.value("playback/mvAudioFallbackEnabled").toBool(),
                         true);
                if (!captures.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/settings-%1-%2.png").arg(int(mode)).arg(zoom)));
                QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
                QTRY_VERIFY(dialog.isNull());
                playback.preferences()->setMvAudioFallbackEnabled(false);
                for (const auto &kind : {QString("cloud"), QString("mv")}) {
                    PlaybackServiceTestAccess::sourcePresentation(playback, kind);
                    QVERIFY(QMetaObject::invokeMethod(
                        quality, "open", Q_ARG(QVariant, (QVariantMap{{"kind", "quality"}}))));
                    QPointer<QObject> popup = quality->property("item").value<QObject *>();
                    QVERIFY(popup);
                    QTRY_VERIFY(popup->property("opened").toBool());
                    QQuickItem *cloud = nullptr;
                    QTRY_VERIFY(
                        (cloud = visual(session.window->contentItem(), "playbackOption-cloud")));
                    QVERIFY(cloud->isVisible());
                    QCOMPARE(cloud->property("selected").toBool(), kind == "cloud");
                    QVERIFY(playback.sourceNotice().contains(kind == "mv" ? "MV" : "云盘"));
                    if (!captures.isEmpty()) {
                        QSignalSpy redraw(session.window, &QQuickWindow::frameSwapped);
                        session.window->update();
                        QTRY_VERIFY(!redraw.isEmpty());
                        QVERIFY(session.window->grabWindow().save(
                            captures +
                            QString("/%1-%2-%3.png").arg(kind).arg(int(mode)).arg(zoom)));
                    }
                    QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                    QTRY_VERIFY(popup.isNull());
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void lyricFontSettingsApplyToProductionWindows()
    {
        Session session;
        session.initialRoute = "themes";
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.router.shutdown();
            session.commands.shutdown();
            session.windows.shutdown();
            session.root.reset();
            session.window = nullptr;
        });
        session.theme.setMotionEnabled(false);
        session.shell.setMotionEnabled(false);
        session.window->resize(1250, 900);
        QString family;
        for (const auto &candidate :
             {"Courier New", "DejaVu Sans Mono", "Liberation Mono", "Arial"}) {
            if (QFontDatabase::hasFamily(candidate)) {
                family = candidate;
                break;
            }
        }
        QVERIFY(!family.isEmpty());
        auto *owner = session.root->findChild<QObject *>("settingsDialogOwner");
        QVERIFY(owner);
        const auto captures = qEnvironmentVariable("ECHO_LYRIC_FONT_CAPTURES");
        if (!captures.isEmpty())
            QVERIFY(QDir().mkpath(captures));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const int zoom : {100, 125}) {
                session.shell.resetZoom();
                for (int step = 100; step < zoom; step += 5)
                    session.shell.zoomIn();
                const QVariantMap properties{{"initialSection", "font"}};
                QVERIFY(QMetaObject::invokeMethod(owner, "open", Q_ARG(QVariant, properties)));
                QPointer<QObject> dialog = owner->property("item").value<QObject *>();
                QVERIFY(dialog);
                QTRY_VERIFY(dialog->property("opened").toBool());
                QQuickItem *globalReset = nullptr;
                QTRY_VERIFY((globalReset =
                                 visual(session.window->contentItem(), "settingsGlobalFontReset")));
                QVERIFY(!globalReset->isVisible());
                session.theme.setGlobalFont(family);
                QTRY_VERIFY(globalReset->isVisible() && globalReset->width() > 20);
                QVERIFY(Test::prepareNativeInput(session.window));
                QSignalSpy resetFrames(session.window, &QQuickWindow::frameSwapped);
                session.window->update();
                QTRY_VERIFY(!resetFrames.isEmpty());
                QTest::mouseClick(
                    session.window, Qt::LeftButton, Qt::NoModifier,
                    globalReset->mapToScene({globalReset->width() / 2, globalReset->height() / 2})
                        .toPoint());
                QTRY_COMPARE(session.theme.globalFont(), QString("system-ui"));
                QTRY_VERIFY(!globalReset->isVisible());
                for (const auto &name :
                     {"settingsLyricFont", "settingsDesktopLyricFont", "settingsMiniLyricFont"}) {
                    QQuickItem *select = nullptr;
                    QTRY_VERIFY((select = visual(session.window->contentItem(), name)));
                    QTRY_VERIFY(select->isVisible() && select->width() > 100);
                    QCOMPARE(select->property("currentIndex").toInt(), 0);
                    QVERIFY(!select->property("popup").value<QObject *>());
                    QVERIFY(Test::prepareNativeInput(session.window));
                    QTest::mouseClick(
                        session.window, Qt::LeftButton, Qt::NoModifier,
                        select->mapToScene({select->width() / 2, select->height() / 2}).toPoint());
                    QPointer<QObject> popup;
                    QTRY_VERIFY((popup = select->property("popup").value<QObject *>()));
                    QTRY_VERIFY(popup->property("opened").toBool());
                    QInputMethodEvent familyInput;
                    familyInput.setCommitString(family);
                    QVERIFY(session.window->activeFocusItem());
                    QCoreApplication::sendEvent(session.window->activeFocusItem(), &familyInput);
                    const auto sourceIndex = session.theme.fontFamilies().indexOf(family) + 2;
                    QQuickItem *option = nullptr;
                    QTRY_VERIFY((option = visual(session.window->contentItem(),
                                                 "selectOption-" + QString::number(sourceIndex))));
                    QTRY_VERIFY(option->isVisible());
                    QTest::mouseClick(
                        session.window, Qt::LeftButton, Qt::NoModifier,
                        option->mapToScene({option->width() / 2, option->height() / 2}).toPoint());
                    QTRY_VERIFY(popup.isNull());
                    QTRY_COMPARE(select->property("currentText").toString(), family);
                }
                QCOMPARE(session.theme.lyricFont(), family);
                QCOMPARE(session.theme.desktopLyricFont(), family);
                QCOMPARE(session.theme.miniLyricFont(), family);
                if (!captures.isEmpty()) {
                    QTest::qWait(100);
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/settings-%1-%2.png")
                                       .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                       .arg(zoom)));
                }
                for (const auto &name : {"settingsLyricFontReset", "settingsDesktopLyricFontReset",
                                         "settingsMiniLyricFontReset"}) {
                    auto *reset = visual(session.window->contentItem(), name);
                    QVERIFY(reset && reset->isVisible());
                    QTest::mouseClick(
                        session.window, Qt::LeftButton, Qt::NoModifier,
                        reset->mapToScene({reset->width() / 2, reset->height() / 2}).toPoint());
                }
                QCOMPARE(session.theme.lyricFont(), QString("follow"));
                QCOMPARE(session.theme.desktopLyricFont(), QString("follow"));
                QCOMPARE(session.theme.miniLyricFont(), QString("follow"));
                QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
                QTRY_VERIFY(dialog.isNull());
            }
        }
        PlaybackServiceTestAccess::quality(playback, "flac");
        playback.lyricText()->setWantTranslation(true);
        QVERIFY(playback.lyrics()->replace(
            {{"lines", QJsonArray{QJsonObject{{"text", QStringLiteral("夜色温柔 Music Lyrics")},
                                              {"start_ms", 0},
                                              {"duration_ms", 5000},
                                              {"translation", "Stay close to the music"},
                                              {"words", QJsonArray{}}}}}}));
        session.theme.setGlobalFont(family);
        session.theme.setLyricFont("system-ui");
        session.theme.setDesktopLyricFont(family);
        session.theme.setMiniLyricFont("system-ui");
        auto *pageOwner = session.root->findChild<QObject *>("playbackPageOwner");
        QVERIFY(pageOwner);
        QVERIFY(
            QMetaObject::invokeMethod(pageOwner, "open", Q_ARG(QVariant, QVariant(QVariantMap{}))));
        QQuickItem *pageLyrics = nullptr;
        QTRY_VERIFY((pageLyrics = visual(session.window->contentItem(), "playbackLyricViewport")));
        QTRY_COMPARE(pageLyrics->property("baseFont").value<QFont>().families(),
                     uiFont("system-ui").families());
        const auto findWindow = [](const QString &name) -> QQuickWindow * {
            for (auto *candidate : QGuiApplication::topLevelWindows())
                if (candidate->objectName() == name)
                    return qobject_cast<QQuickWindow *>(candidate);
            return nullptr;
        };
        const auto lyricFontsMatch = [](QQuickItem *root, const QFont &expected) {
            int count = 0;
            bool matches = true;
            const auto visit = [&](auto &&self, QQuickItem *item) -> void {
                const auto font = item->property("font");
                if (item->isVisible() && font.canConvert<QFont>() &&
                    !item->property("text").toString().isEmpty()) {
                    ++count;
                    matches = matches && font.value<QFont>().families() == expected.families();
                }
                for (auto *child : item->childItems())
                    self(self, child);
            };
            visit(visit, root);
            return count > 0 && matches;
        };
        QTRY_VERIFY(lyricFontsMatch(pageLyrics, uiFont("system-ui")));
        QTRY_VERIFY(pageOwner->property("item").value<QObject *>()->property("opened").toBool());
        if (!captures.isEmpty()) {
            QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
            session.window->update();
            QTRY_VERIFY(!frames.isEmpty());
            QVERIFY(session.window->grabWindow().save(captures + "/playback.png"));
        }
        session.theme.setLyricFont(family);
        QTRY_VERIFY(lyricFontsMatch(pageLyrics, uiFont(family)));
        session.theme.setLyricFont("system-ui");
        QTRY_VERIFY(lyricFontsMatch(pageLyrics, uiFont("system-ui")));
        auto *floating = session.windows.floating();
        floating->setDesktopEnabled(true);
        QTRY_VERIFY(findWindow("desktopLyricWindow"));
        QPointer<QQuickWindow> desktop = findWindow("desktopLyricWindow");
        auto *desktopLyrics = visual(desktop->contentItem(), "desktopLyricViewport");
        QVERIFY(desktopLyrics);
        QTRY_COMPARE(desktopLyrics->property("baseFont").value<QFont>().families(),
                     uiFont(family).families());
        QTRY_VERIFY(lyricFontsMatch(desktopLyrics, uiFont(family)));
        floating->showMini();
        floating->setPanel("lyric");
        QTRY_VERIFY(findWindow("miniPlayerWindow"));
        QPointer<QQuickWindow> mini = findWindow("miniPlayerWindow");
        QQuickItem *miniLyrics = nullptr;
        QTRY_VERIFY((miniLyrics = visual(mini->contentItem(), "miniLyricViewport")));
        QTRY_COMPARE(miniLyrics->property("baseFont").value<QFont>().families(),
                     uiFont("system-ui").families());
        session.theme.setMiniLyricFont(family);
        QTRY_COMPARE(miniLyrics->property("baseFont").value<QFont>().families(),
                     uiFont(family).families());
        QTRY_VERIFY(lyricFontsMatch(miniLyrics, uiFont(family)));
        session.theme.setGlobalFont("system-ui");
        QCOMPARE(desktopLyrics->property("baseFont").value<QFont>().families(),
                 uiFont(family).families());
        if (!captures.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(desktop->grabWindow().save(captures + "/desktop.png"));
            QVERIFY(mini->grabWindow().save(captures + "/mini.png"));
        }
        floating->closeMini();
        floating->setDesktopEnabled(false);
        QTRY_VERIFY(mini.isNull() && desktop.isNull());
        floating->showMini();
        floating->setPanel("lyric");
        QTRY_VERIFY(findWindow("miniPlayerWindow"));
        QTRY_VERIFY((miniLyrics = visual(findWindow("miniPlayerWindow")->contentItem(),
                                         "miniLyricViewport")));
        QTRY_COMPARE(miniLyrics->property("baseFont").value<QFont>().families(),
                     uiFont(family).families());
        session.theme.setMiniLyricFont("follow");
        session.theme.setGlobalFont(family);
        QTRY_VERIFY(lyricFontsMatch(miniLyrics, session.theme.applicationFont()));
        session.theme.setGlobalFont("system-ui");
        QTRY_VERIFY(lyricFontsMatch(miniLyrics, uiFont("system-ui")));
        QCOMPARE(warnings.size(), 0);
    }
    void lyricVariantSettingUsesProductionSelectAndReleasesPopup()
    {
        Session session;
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.theme.setMotionEnabled(false);
        session.shell.setMotionEnabled(false);
        session.window->resize(1250, 900);
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *owner = session.root->findChild<QObject *>("settingsDialogOwner");
        QVERIFY(owner);
        const auto artifacts = qEnvironmentVariable("ECHO_VARIANT_ARTIFACTS");
        if (!artifacts.isEmpty())
            QVERIFY(QDir().mkpath(artifacts));
        for (const auto mode : {ThemeStore::Dark, ThemeStore::Light}) {
            session.theme.setMode(mode);
            for (const int zoom : {100, 125}) {
                session.shell.resetZoom();
                for (int step = 100; step < zoom; step += 5)
                    session.shell.zoomIn();
                const QVariantMap properties{{"initialSection", "lyric"}};
                QVERIFY(QMetaObject::invokeMethod(owner, "open", Q_ARG(QVariant, properties)));
                QPointer<QObject> dialog = owner->property("item").value<QObject *>();
                QVERIFY(dialog);
                QTRY_VERIFY(dialog->property("opened").toBool());
                auto *sectionLoader =
                    visual(session.window->contentItem(), "settingsSectionLoader");
                QVERIFY(sectionLoader);
                // Asynchronous Loader children exist before their layout is ready for input.
                QTRY_COMPARE_WITH_TIMEOUT(sectionLoader->property("status").toInt(), 1, 15000);
                QQuickItem *select = nullptr;
                QTRY_VERIFY(
                    (select = visual(session.window->contentItem(), "lyricTextConversion")));
                QTRY_VERIFY(select->isVisible());
                QVERIFY(!select->property("popup").value<QObject *>());
                playback.lyricText()->setConversion("traditional");
                QTRY_COMPARE(select->property("currentIndex").toInt(), 2);
                QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                session.window->update();
                QTRY_VERIFY(!frames.isEmpty());
                auto *viewport = visual(session.window->contentItem(), "settingsContentViewport");
                QVERIFY(viewport);
                const auto local = select->mapToItem(viewport, {0, 0});
                if (local.y() < 0 || local.y() + select->height() > viewport->height()) {
                    const auto maximum = qMax(0., viewport->property("contentHeight").toDouble() -
                                                      viewport->height());
                    viewport->setProperty("contentY",
                                          qBound(0.,
                                                 viewport->property("contentY").toDouble() +
                                                     local.y() - viewport->height() / 2,
                                                 maximum));
                }
                QVERIFY(Test::prepareNativeInput(session.window));
                const auto point =
                    select->mapToScene({select->width() / 2, select->height() / 2}).toPoint();
                Test::moveNativePointer(session.window, point);
                if (!artifacts.isEmpty())
                    QVERIFY(session.window->grabWindow().save(artifacts +
                                                              "/settings-before-select.png"));
                qInfo() << "Conversion select input" << point << "size" << select->size();
                QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
                QPointer<QObject> popup;
                QTRY_VERIFY((popup = select->property("popup").value<QObject *>()));
                QTRY_VERIFY(popup->property("opened").toBool());
                auto *list = select->property("optionList").value<QObject *>();
                QVERIFY(list);
                QTRY_COMPARE(list->property("count").toInt(), 6);
                QQuickItem *option = nullptr;
                QTRY_VERIFY((option = visual(session.window->contentItem(), "selectOption-3")) &&
                            option->isVisible());
                if (!artifacts.isEmpty()) {
                    QTest::qWait(100);
                    QVERIFY(session.window->grabWindow().save(
                        artifacts + QString("/variants-%1-%2.png")
                                        .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                        .arg(zoom)));
                }
                QTest::mouseClick(
                    session.window, Qt::LeftButton, Qt::NoModifier,
                    option->mapToScene({option->width() / 2, option->height() / 2}).toPoint());
                QTRY_COMPARE(playback.lyricText()->conversion(),
                             QStringLiteral("traditional-variants"));
                QTRY_COMPARE(select->property("currentIndex").toInt(), 3);
                QCOMPARE(select->property("currentText").toString(), QStringLiteral("转异体繁体"));
                QCOMPARE(session.preferences.value("lyrics/conversion").toString(),
                         QStringLiteral("traditional-variants"));
                QTRY_VERIFY(popup.isNull());
                QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
                QTRY_VERIFY(dialog.isNull());
            }
        }
        QCOMPARE(warnings.size(), 0);
        playback.shutdown();
    }
    void productionPopupsFollowRequestOwnership()
    {
        Session session;
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.theme.setMotionEnabled(false);
        session.shell.setMotionEnabled(false);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profileEdit"));
        Test::moveNativePointer(session.window, {400, 400});
        const auto popupCount = [&] {
            QSet<QObject *> visited;
            int count = 0;
            const auto visit = [&](auto &&self, QObject *object) -> void {
                if (!object || visited.contains(object))
                    return;
                visited.insert(object);
                count += object->inherits("QQuickPopup");
                for (auto *child : object->children())
                    self(self, child);
                if (auto *item = qobject_cast<QQuickItem *>(object))
                    for (auto *child : item->childItems())
                        self(self, child);
            };
            visit(visit, session.root.data());
            return count;
        };
        QTRY_COMPARE(popupCount(), 0);
        QQuickItem anchor(session.window->contentItem());
        anchor.setSize({32, 32});
        anchor.setPosition({600, 500});
        const QStringList cases{
            "settingsDialog",        "taskCenterDialog",      "playbackPage",
            "playbackQueueDrawer",   "playbackEffectPopup",   "sleepTimerPopup",
            "playbackOptionsPopup",  "playerbarMorePopup",    "sidebarLayoutPopup",
            "sidebarAccountPopover", "accountSwitcherDialog", "logoutConfirmation",
            "playlistCreateDialog",  "playlistOrderDialog",   "playlistRemoveDialog",
            "playlistImportDialog",  "profileEditorDialog",   "profileGradeDialog",
            "profileSocialDrawer",   "profileDeviceDialog",   "listeningPreferencesDialog",
            "contentBlacklistDialog"};
        for (const auto &name : cases) {
            auto *owner = session.root->findChild<QObject *>(name + "Owner");
            if (!owner) {
                auto *page = visual(session.window->contentItem(), "profilePage");
                if (page)
                    owner = page->findChild<QObject *>(name + "Owner");
            }
            QVERIFY2(owner, qPrintable(name));
            QVERIFY2(!owner->property("item").value<QObject *>(), qPrintable(name));
            QVariantMap properties;
            if (name.contains("Popup"))
                properties.insert("anchorItem", QVariant::fromValue(&anchor));
            for (int cycle = 0; cycle < 2; ++cycle) {
                QVERIFY(QMetaObject::invokeMethod(owner, "open", Q_ARG(QVariant, properties)));
                QPointer<QObject> popup = owner->property("item").value<QObject *>();
                QVERIFY2(popup, qPrintable(name));
                QTRY_VERIFY(popup->property("opened").toBool());
                QTRY_COMPARE(popupCount(), 1);
                QPointer<QObject> content = popup->property("contentItem").value<QObject *>();
                QVERIFY(content);
                QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                QTRY_VERIFY(popup.isNull());
                QTRY_VERIFY(content.isNull());
                QTRY_COMPARE(popupCount(), 0);
            }
            qInfo() << "Released production popup" << name;
        }
        // Service-side reopen signals must survive the previous UI instance.
        for (int cycle = 0; cycle < 3; ++cycle) {
            session.accounts.library.importer()->reopen();
            QPointer<QObject> popup = session.root->findChild<QObject *>("playlistImportDialog");
            QVERIFY(popup);
            QTRY_VERIFY(popup->property("opened").toBool());
            QVERIFY(QMetaObject::invokeMethod(popup, "close"));
            QTRY_VERIFY(popup.isNull());
            playback.libraryActions()->dialogRequested();
            popup = session.root->findChild<QObject *>("addToPlaylistDialog");
            QVERIFY(popup);
            QTRY_VERIFY(popup->property("opened").toBool());
            playback.libraryActions()->completed();
            QTRY_VERIFY(popup.isNull());
            QTRY_COMPARE(popupCount(), 0);
        }
        auto *importer = session.accounts.library.importer();
        PlaylistImportServiceTestAccess::awaitDuplicate(*importer, "生命周期待确认歌单");
        QTRY_COMPARE(popupCount(), 2);
        QPointer<QObject> pendingImport =
            session.root->findChild<QObject *>("playlistImportDialog");
        QVERIFY(pendingImport);
        session.window->hide();
        QTRY_VERIFY(pendingImport.isNull());
        QTRY_COMPARE(popupCount(), 0);
        QCOMPARE(importer->duplicateName(), QString("生命周期待确认歌单"));
        session.window->show();
        QVERIFY(Test::prepareNativeInput(session.window));
        QTRY_COMPARE(popupCount(), 2);
        pendingImport = session.root->findChild<QObject *>("playlistImportDialog");
        QVERIFY(pendingImport);
        QCOMPARE(pendingImport->property("duplicateDraft").toString(), importer->duplicateName());
        importer->confirmDuplicate(false);
        QTRY_COMPARE(popupCount(), 1);
        QVERIFY(QMetaObject::invokeMethod(pendingImport, "close"));
        QTRY_VERIFY(pendingImport.isNull());
        QTRY_COMPARE(popupCount(), 0);
        QCOMPARE(warnings.size(), 0);
    }
    void floatingLyricsFollowRealAudioAndStopMotionOnPause()
    {
        Session session;
        session.theme.setMotionEnabled(true);
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.router.shutdown();
            session.commands.shutdown();
            session.windows.shutdown();
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        const auto path = session.directory.filePath("floating-motion.wav");
        QVERIFY(writeFloatingTestWave(path));
        PlaybackTrack track;
        track.id = path;
        track.localPath = path;
        track.title = QStringLiteral("晚风里的旋律");
        track.artist = QStringLiteral("EchoMusicNext 原生播放验证");
        track.durationMs = 16000;
        playback.setVolume(0);
        playback.playTracks({track}, 0, "Native motion", "floating-motion");
        QTRY_VERIFY_WITH_TIMEOUT(playback.playing(), 15000);
        QJsonArray rows;
        const QStringList lyrics{
            QStringLiteral("让音乐留在身边"),      QStringLiteral("像晚风 轻轻掠过心间"),
            QStringLiteral("每一刻 都清晰可见"),   QStringLiteral("下一句 温柔地浮现"),
            QStringLiteral("暂停后 停在这一瞬间"), QStringLiteral("随着旋律 继续向前")};
        for (int i = 0; i < lyrics.size(); ++i)
            rows.append(QJsonObject{{"text", lyrics[i]},
                                    {"start_ms", i * 1500},
                                    {"duration_ms", 1500},
                                    {"words", QJsonArray{}},
                                    {"translation", "Let the music stay with you"},
                                    {"romanization", "rang yin yue liu zai shen bian"}});
        QVERIFY(playback.lyrics()->replace({{"lines", rows}}));
        auto *floating = session.windows.floating();
        floating->setDesktopEnabled(true);
        QQuickWindow *desktop = nullptr;
        QTRY_VERIFY(([&] {
            for (auto *candidate : QGuiApplication::topLevelWindows())
                if (candidate->objectName() == "desktopLyricWindow")
                    desktop = qobject_cast<QQuickWindow *>(candidate);
            return desktop != nullptr;
        })());
        auto *viewport = visual(desktop->contentItem(), "desktopLyricViewport");
        QVERIFY(viewport);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        auto *probe = new FloatingFrameProbe(desktop);
        QTRY_VERIFY_WITH_TIMEOUT(viewport->property("arrival").toReal() > .15, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(viewport->property("arrival").toReal() < .15, 1500);
        QVERIFY(playback.playing());
        const auto before = playback.positionMs();
        floating->toggleMini();
        QTRY_VERIFY(floating->miniActive());
        floating->setPanel("lyric");
        QTest::qWait(350);
        if (!qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR").isEmpty()) {
            for (auto *candidate : QGuiApplication::topLevelWindows())
                if (candidate->objectName() == "miniPlayerWindow")
                    QVERIFY(qobject_cast<QQuickWindow *>(candidate)->grabWindow().save(
                        qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR") +
                        "/mini-real-audio.png"));
        }
        QVERIFY(playback.playing());
        QVERIFY(playback.positionMs() > before);
        floating->showMain();
        playback.pause();
        QTRY_VERIFY(!playback.playing());
        QTRY_VERIFY(qAbs(viewport->property("arrival").toReal()) < .001);
        // A final engine telemetry sample can arrive after the pause command.
        QTest::qWait(300);
        const auto paused = viewport->property("renderTime").toReal();
        QTest::qWait(100);
        QVERIFY(qAbs(viewport->property("renderTime").toReal() - paused) < 1);
        session.theme.setMotionEnabled(false);
        playback.play();
        QTRY_VERIFY(playback.playing());
        playback.seek(6000);
        QTRY_VERIFY(playback.positionMs() >= 5900);
        QTRY_VERIFY(qAbs(viewport->property("arrival").toReal()) < .001);
        const auto capture = qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(desktop->grabWindow().save(capture + "/desktop-real-audio.png"));
        if (qEnvironmentVariableIntValue("ECHO_FLOATING_PERF"))
            qInfo() << "floating-audio-render frames=" << probe->frames()
                    << "max-sync-ms=" << probe->maximumSyncMs()
                    << "max-render-record-ms=" << probe->maximumMs()
                    << "rss-kib=" << floatingRssKiB();
        floating->setDesktopEnabled(false);
        playback.stop();
        QCOMPARE(warnings.size(), 0);
    }
    void miniWidthPreferencesClampAndRestore_data()
    {
        QTest::addColumn<int>("savedWidth");
        QTest::addColumn<int>("expectedWidth");
        QTest::newRow("below-minimum") << 200 << 360;
        QTest::newRow("within-range") << 480 << 480;
        QTest::newRow("above-maximum") << 900 << 560;
    }
    void miniWidthPreferencesClampAndRestore()
    {
        QFETCH(int, savedWidth);
        QFETCH(int, expectedWidth);
        QTemporaryDir directory;
        QSettings settings(directory.filePath("mini.ini"), QSettings::IniFormat);
        const auto area = QGuiApplication::primaryScreen()->availableGeometry();
        settings.setValue("floating/mini/anchor",
                          QRect(area.topLeft() + QPoint(40, 40), QSize(savedWidth, 64)));
        {
            QQuickWindow mini;
            FloatingPlayerController floating(settings, true);
            floating.toggleMini();
            floating.attachMini(&mini);
            QTRY_COMPARE(mini.width(), expectedWidth);
            QCOMPARE(mini.height(), 64);
            floating.setPanel("lyric");
            QTRY_COMPARE(mini.height(), 360);
            QCOMPARE(mini.width(), expectedWidth);
            floating.setPanel("");
            QTRY_COMPARE(mini.height(), 64);
            QCOMPARE(mini.width(), expectedWidth);
            mini.setWidth(500);
            floating.attachMini(nullptr);
            floating.shutdown();
        }
        QCOMPARE(settings.value("floating/mini/anchor").toRect().width(), 500);
        {
            QQuickWindow mini;
            FloatingPlayerController floating(settings, true);
            floating.toggleMini();
            floating.attachMini(&mini);
            QTRY_COMPARE(mini.width(), 500);
            QCOMPARE(mini.height(), 64);
            floating.attachMini(nullptr);
            floating.shutdown();
        }
    }
    void miniProgressVolumeBackdropFavoriteAndDesktopBadge_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }
    void miniProgressVolumeBackdropFavoriteAndDesktopBadge()
    {
        QFETCH(bool, dark);
        Session session;
        session.theme.setMode(dark ? ThemeStore::Dark : ThemeStore::Light);
        session.theme.setMotionEnabled(false);
        session.theme.setAccent(QColor("#0088ff"));
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.router.shutdown();
            session.commands.shutdown();
            session.windows.shutdown();
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        const auto path = session.directory.filePath("mini-controls.wav");
        QVERIFY(writeFloatingTestWave(path));
        QImage art(800, 800, QImage::Format_RGB32);
        art.fill(Qt::red);
        const auto artPath = session.directory.filePath("mini-cover.png");
        QVERIFY(art.save(artPath));
        PlaybackTrack track;
        track.id = path;
        track.localPath = path;
        track.title = QStringLiteral("半壶纱.mp3 Mini 原生交互与长标题回归");
        track.artist = QStringLiteral("刘珂矣 Artist 字体回退与长名称回归");
        track.durationMs = 16000;
        track.cover = QUrl::fromLocalFile(artPath).toString();
        playback.setVolume(0);
        playback.playTracks({track}, 0, "Mini controls", "mini-controls");
        QTRY_VERIFY_WITH_TIMEOUT(playback.playing(), 15000);
        playback.pause();
        QTRY_VERIFY(!playback.playing());
        auto *floating = session.windows.floating();
        QQuickItem *desktopAction = nullptr;
        QTRY_VERIFY((desktopAction =
                         visual(session.window->contentItem(), "playerbar-action-desktop-lyric")));
        auto *badge = visual(desktopAction, "actionBadge");
        QVERIFY(badge);
        QTRY_COMPARE(badge->property("count").toString(), QString("OFF"));
        floating->setDesktopEnabled(true);
        QTRY_COMPARE(badge->property("count").toString(), QString("ON"));
        QVERIFY(badge->isVisible());
        QVERIFY(desktopAction->property("actionActive").toBool());
        QVERIFY(session.commands.playerbarActions()->setBadgeVisible("host/desktop-lyric", false));
        QTRY_VERIFY(!badge->isVisible());
        QVERIFY(session.commands.playerbarActions()->setBadgeVisible("host/desktop-lyric", true));
        QTRY_VERIFY(badge->isVisible());

        floating->setDesktopEnabled(false);
        floating->toggleMini();
        QQuickWindow *mini = nullptr;
        QTRY_VERIFY(([&] {
            for (auto *candidate : QGuiApplication::topLevelWindows())
                if (candidate->objectName() == "miniPlayerWindow")
                    mini = qobject_cast<QQuickWindow *>(candidate);
            return mini != nullptr;
        })());
        QVERIFY(Test::prepareNativeInput(mini));
        QCOMPARE(mini->minimumWidth(), 360);
        QCOMPARE(mini->maximumWidth(), 560);
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() == "cocoa") {
            QCOMPARE(nativeFloatingAppearance(mini).value("minimumContentWidth").toInt(), 360);
            QCOMPARE(nativeFloatingAppearance(mini).value("maximumContentWidth").toInt(), 560);
        }
#endif
        if (QGuiApplication::platformName() == "cocoa") {
            const auto dragWidth = [&](Qt::Edge edge, int delta) {
                auto *handle =
                    visual(mini->contentItem(), "miniPlayerResize-" + QString::number(int(edge)));
                QVERIFY(handle);
                const auto start = handle->mapToScene({2, 24}).toPoint();
                const auto endGlobal = mini->mapToGlobal(start) + QPoint(delta, 0);
                Test::moveNativePointer(mini, start);
                QTest::mousePress(mini, Qt::LeftButton, Qt::NoModifier, start);
                Test::moveNativePointer(mini, mini->mapFromGlobal(endGlobal));
                QTest::mouseRelease(mini, Qt::LeftButton, Qt::NoModifier,
                                    mini->mapFromGlobal(endGlobal));
            };
            dragWidth(Qt::RightEdge, 300);
            QTRY_COMPARE(mini->width(), 560);
            dragWidth(Qt::RightEdge, -400);
            QTRY_COMPARE(mini->width(), 360);
            const auto right = mini->geometry().right();
            dragWidth(Qt::LeftEdge, -300);
            QTRY_COMPARE(mini->width(), 560);
            QCOMPARE(mini->geometry().right(), right);
            dragWidth(Qt::LeftEdge, 400);
            QTRY_COMPARE(mini->width(), 360);
            QCOMPARE(mini->geometry().right(), right);
            QCOMPARE(mini->height(), 64);
        }
        mini->setWidth(480);
        QSignalSpy resizedFrame(mini, &QQuickWindow::frameSwapped);
        mini->update();
        QTRY_VERIFY(!resizedFrame.isEmpty());
        QTRY_COMPARE(mini->width(), 480);
        auto *controls = visual(mini->contentItem(), "miniPlayerControls");
        auto *progress = visual(mini->contentItem(), "miniPlayerProgress");
        QVERIFY(controls && progress);
        Test::moveNativePointer(mini, {-12, -12});
        QTRY_VERIFY(!mini->property("showControls").toBool());
        auto *metadata = visual(mini->contentItem(), "miniPlayerMetadata");
        auto *title = visual(mini->contentItem(), "miniPlayerTitle");
        auto *artist = visual(mini->contentItem(), "miniPlayerArtist");
        QVERIFY(metadata && title && artist);
        for (int width : {360, 560}) {
            mini->setWidth(width);
            QSignalSpy frame(mini, &QQuickWindow::frameSwapped);
            mini->update();
            QTRY_VERIFY(!frame.isEmpty());
            QCOMPARE(metadata->height(), 34.);
            QCOMPARE(title->height(), 18.);
            QCOMPARE(artist->height(), 15.);
            QCOMPARE(title->property("font").value<QFont>().pixelSize(), 13);
            QCOMPARE(artist->property("font").value<QFont>().pixelSize(), 11);
            const auto image = mini->grabWindow();
            const qreal ratio = image.width() / qreal(mini->width());
            const auto rowBounds = artist->mapRectToScene(artist->boundingRect());
            const auto trackTop = progress->mapToScene({0, 6}).y();
            // Include the gap: fallback ink overflowing its line box must be
            // detected too. Stop before the rail's antialiased top edge.
            const auto pixels =
                QRectF(rowBounds.topLeft() * ratio,
                       QSizeF(rowBounds.width(), trackTop - rowBounds.y() - 1) * ratio)
                    .toAlignedRect()
                    .intersected(image.rect());
            const auto base = session.theme.elevated();
            int inkBottom = -1;
            for (int y = pixels.top(); y <= pixels.bottom(); ++y)
                for (int x = pixels.left(); x <= pixels.right(); ++x) {
                    const auto p = image.pixelColor(x, y);
                    if (std::max({qAbs(p.red() - base.red()), qAbs(p.green() - base.green()),
                                  qAbs(p.blue() - base.blue())}) > 25)
                        inkBottom = qMax(inkBottom, y);
                }
            QVERIFY(inkBottom >= 0);
            QVERIFY2(trackTop - inkBottom / ratio >= 4., "Artist ink must clear the progress rail");
            const auto capture = qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR");
            if (!capture.isEmpty())
                QVERIFY(image.save(
                    capture +
                    QString("/mini-metadata-%1-%2.png").arg(dark ? "dark" : "light").arg(width)));
        }
        mini->setWidth(480);
        QSignalSpy metadataFrame(mini, &QQuickWindow::frameSwapped);
        mini->update();
        QTRY_VERIFY(!metadataFrame.isEmpty());
        auto *seek = visual(progress, "playbackSeekSlider");
        QVERIFY(seek);
        QCOMPARE(mini->height(), 64);
        QCOMPARE(progress->x(), 91.);
        QCOMPARE(progress->y(), 43.);
        QVERIFY(progress->y() + progress->height() <= 59.);
        const auto scrub = [&] {
            const auto start = seek->mapToScene({seek->width() * .2, seek->height() / 2}).toPoint();
            const auto end = seek->mapToScene({seek->width() * .7, seek->height() / 2}).toPoint();
            Test::moveNativePointer(mini, start);
            QTest::mousePress(mini, Qt::LeftButton, Qt::NoModifier, start);
            QTest::mouseMove(mini, end);
            QTest::mouseRelease(mini, Qt::LeftButton, Qt::NoModifier, end);
        };
        scrub();
        QTRY_VERIFY2_WITH_TIMEOUT(
            playback.positionMs() > 9000 && playback.positionMs() < 12500,
            qPrintable(QString("position=%1 width=%2 progressWidth=%3 sliderWidth=%4 pressed=%5")
                           .arg(playback.positionMs())
                           .arg(mini->width())
                           .arg(progress->width())
                           .arg(seek->width())
                           .arg(seek->property("pressed").toBool())),
            5000);
        QCOMPARE(mini->height(), 64);
        auto *favorite = visual(mini->contentItem(), "miniPlayerFavorite");
        QVERIFY(favorite);
        PlaybackLibraryActionsTestAccess::favorite(*playback.libraryActions(),
                                                   playback.currentTrack(), true);
        QTRY_COMPARE(favorite->property("foreground").value<QColor>(), QColor("#ef4444"));
        Test::moveNativePointer(mini, favorite->mapToScene({12, 15}).toPoint());
        QTRY_COMPARE(favorite->property("foreground").value<QColor>(), QColor("#ef4444"));
        auto *volume = visual(mini->contentItem(), "miniPlayerVolume");
        auto *popup = mini->findChild<QObject *>("miniPlayerVolumePopup");
        QVERIFY(volume && popup);
        playback.setVolume(50);
        const auto click = [](QQuickWindow *window, QQuickItem *item) {
            const auto p = item->mapToScene({item->width() / 2, item->height() / 2}).toPoint();
            Test::moveNativePointer(window, p);
            QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, p);
        };
        Test::moveNativePointer(mini, volume->mapToScene({12, 15}).toPoint());
        QTest::qWait(80);
        QVERIFY(!popup->property("visible").toBool());
        click(mini, volume);
        QTRY_VERIFY(popup->property("opened").toBool());
        QCOMPARE(playback.volume(), 50.);
        QCOMPARE(mini->height(), 64);
        QCOMPARE(popup->property("height").toReal(), 57.);
        QCOMPARE(popup->property("width").toReal(), 176.);
        auto *popupContent = popup->property("contentItem").value<QQuickItem *>();
        auto *volumeSlider = visual(popupContent, "playbackVolumeSlider");
        auto *mute = visual(popupContent, "playbackVolumeMute");
        QVERIFY(volumeSlider && mute);
        QCOMPARE(volumeSlider->property("orientation").toInt(), int(Qt::Horizontal));
        auto *popupWindow = volumeSlider->window();
        QVERIFY(popupWindow);
        if (QGuiApplication::platformName() != "offscreen") {
            QVERIFY(popupWindow != mini);
            QTRY_VERIFY(QTest::qWaitForWindowExposed(popupWindow));
            QVERIFY(popupWindow->screen()->availableGeometry().contains(popupWindow->geometry()));
#ifdef Q_OS_MACOS
            if (QGuiApplication::platformName() == "cocoa") {
                for (bool pinned : {true, false, true}) {
                    floating->mini()->setAlwaysOnTop(pinned);
                    if (!popup->property("visible").toBool())
                        click(mini, volume);
                    QTRY_VERIFY(popup->property("opened").toBool());
                    popupWindow = volumeSlider->window();
                    QVERIFY(popupWindow);
                    QCOMPARE(popupWindow->transientParent(), mini);
                    mini->raise();
                    const auto hostNumber =
                        nativeFloatingAppearance(mini).value("windowNumber").toLongLong();
                    QTRY_VERIFY(nativeFloatingAppearance(popupWindow).value("level").toLongLong() >
                                nativeFloatingAppearance(mini).value("level").toLongLong());
                    QTRY_VERIFY(
                        nativeMacComposition(popupWindow, hostNumber).value("aboveHost").toBool());
                    QCOMPARE(
                        nativeFloatingAppearance(popupWindow).value("allApplications").toBool(),
                        pinned);
                    if (!pinned)
                        QVERIFY(nativeFloatingAppearance(popupWindow).value("level").toLongLong() <
                                1000);
                }
            }
#endif
        }
        const auto volumePoint =
            volumeSlider->mapToScene({volumeSlider->width() * .8, volumeSlider->height() / 2})
                .toPoint();
        Test::moveNativePointer(popupWindow, volumePoint);
        QTest::mouseClick(popupWindow, Qt::LeftButton, Qt::NoModifier, volumePoint);
        QTRY_VERIFY(playback.volume() > 70);
        const auto capture = qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(popupWindow->grabWindow().save(
                capture + (dark ? "/mini-volume-dark.png" : "/mini-volume-light.png")));
        click(popupWindow, mute);
        QTRY_COMPARE(playback.volume(), 0.);
        click(popupWindow, mute);
        QTRY_VERIFY(playback.volume() > 70);
        Test::moveNativePointer(mini, {16, 55});
        QTest::mouseClick(mini, Qt::LeftButton, Qt::NoModifier, {16, 55});
        QTRY_VERIFY(!popup->property("visible").toBool());
        click(mini, volume);
        QTRY_VERIFY(popup->property("opened").toBool());
        mini->showMinimized();
        QTRY_VERIFY(!popup->property("visible").toBool());
        mini->showNormal();
        QVERIFY(Test::prepareNativeInput(mini));
        const auto originalPosition = mini->position();
        const auto area = mini->screen()->availableGeometry();
        mini->setPosition(area.left() + 80, area.top() + 4);
        if (!popup->property("visible").toBool())
            click(mini, volume);
        QTRY_VERIFY(!popup->property("placedAbove").toBool());
        popupWindow = volumeSlider->window();
        QVERIFY(popupWindow);
        QTest::keyClick(popupWindow, Qt::Key_Escape);
        QTRY_VERIFY(!popup->property("visible").toBool());
        mini->setPosition(area.left() + 80, area.bottom() - 70);
        click(mini, volume);
        QTRY_VERIFY(popup->property("opened").toBool());
        QTRY_VERIFY(popup->property("placedAbove").toBool());
        popupWindow = volumeSlider->window();
        QVERIFY(popupWindow);
        QTest::keyClick(popupWindow, Qt::Key_Escape);
        QTRY_VERIFY(!popup->property("visible").toBool());
        mini->setPosition(originalPosition);
        auto *desktopButton = visual(mini->contentItem(), "miniPlayerDesktopLyric");
        QVERIFY(desktopButton);
        click(mini, desktopButton);
        QTRY_VERIFY(floating->desktopEnabled());
        QTRY_COMPARE(badge->property("count").toString(), QString("ON"));
        click(mini, desktopButton);
        QTRY_COMPARE(badge->property("count").toString(), QString("OFF"));
        floating->setPanel("lyric");
        QTRY_COMPARE(mini->height(), 360);
        QCOMPARE(mini->width(), 480);
        mini->setWidth(560);
        QTRY_COMPARE(mini->width(), 560);
        playback.seek(2000);
        QTRY_VERIFY(playback.positionMs() >= 1900 && playback.positionMs() < 3000);
        scrub();
        QTRY_VERIFY(playback.positionMs() > 9000 && playback.positionMs() < 12500);
        auto *menuAction = visual(mini->contentItem(), "miniPlayerLyricMenuAction");
        auto *menu = mini->findChild<QObject *>("miniPlayerLyricMenu");
        QVERIFY(menuAction && menu);
        click(mini, menuAction);
        QTRY_VERIFY(menu->property("opened").toBool());
        auto *menuContent = menu->property("contentItem").value<QQuickItem *>();
        QVERIFY(menuContent && menuContent->window());
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() == "cocoa") {
            auto *menuWindow = menuContent->window();
            QCOMPARE(menuWindow->transientParent(), mini);
            const auto hostNumber =
                nativeFloatingAppearance(mini).value("windowNumber").toLongLong();
            QTRY_VERIFY(nativeMacComposition(menuWindow, hostNumber).value("aboveHost").toBool());
        }
#endif
        QTest::keyClick(menuContent->window(), Qt::Key_Escape);
        QTRY_VERIFY(!menu->property("visible").toBool());
        QQuickItem *backdrop = nullptr;
        QTRY_VERIFY((backdrop = visual(mini->contentItem(), "miniLyricBackdrop")));
        auto *backdropImage = visual(backdrop, "miniLyricBackdropCover");
        QVERIFY(backdropImage);
        QTRY_COMPARE(backdropImage->property("status").toInt(), 1);
        if (mini->rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
            QTRY_VERIFY(visual(backdrop, "miniLyricBackdropComposite"));
            const auto image = mini->grabWindow();
            const auto ratio = image.width() / qreal(mini->width());
            const auto point = backdrop->mapToScene({2, 80});
            const auto edge =
                image.pixelColor(qRound(point.x() * ratio), qRound(point.y() * ratio));
            const auto center =
                image.pixelColor(qRound((point.x() + 18) * ratio), qRound(point.y() * ratio));
            QVERIFY(qAbs(edge.red() - center.red()) <= 2 &&
                    qAbs(edge.green() - center.green()) <= 2);
            const auto base = session.theme.elevated();
            const auto coverAlpha = .22 * (1 - (.84 + .11 * 80 / backdrop->height()));
            QVERIFY(qAbs(edge.green() - qRound(base.green() * (1 - coverAlpha))) <= 3);
            const auto capture = qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR");
            if (!capture.isEmpty())
                QVERIFY(image.save(capture + (dark ? "/mini-fixed-expanded-dark.png"
                                                   : "/mini-fixed-expanded-light.png")));
        }
        floating->mini()->setBackgroundBlur(false);
        QTRY_VERIFY(!visual(mini->contentItem(), "miniLyricBackdrop"));
        floating->mini()->setBackgroundBlur(true);
        QTRY_VERIFY(visual(mini->contentItem(), "miniLyricBackdrop"));
        floating->setPanel("");
        QTRY_COMPARE(mini->height(), 64);
        QCOMPARE(mini->width(), 560);
        QTRY_VERIFY(!visual(mini->contentItem(), "miniLyricBackdrop"));
        click(mini, volume);
        QTRY_VERIFY(popup->property("opened").toBool());
        QPointer<QObject> popupGuard = popup;
        QPointer<QQuickWindow> popupWindowGuard = volumeSlider->window();
        floating->showMain();
        QTRY_VERIFY(popupGuard.isNull());
        QTRY_VERIFY(popupWindowGuard.isNull() || !popupWindowGuard->isVisible());
        floating->toggleMini();
        mini = nullptr;
        QTRY_VERIFY(([&] {
            for (auto *candidate : QGuiApplication::topLevelWindows())
                if (candidate->objectName() == "miniPlayerWindow")
                    mini = qobject_cast<QQuickWindow *>(candidate);
            return mini != nullptr;
        })());
        QTRY_COMPARE(mini->width(), 560);
        floating->showMain();
        playback.stop();
    }
    void lyricSettingsShareCoverPickerAndExcludeTaskbar()
    {
        Session session;
        session.theme.setMotionEnabled(false);
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.router.shutdown();
            session.commands.shutdown();
            session.windows.shutdown();
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        // 弹层按需创建：先经真实入口打开设置，再取得实例。
        session.router.navigate("settings");
        auto *dialog = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(
            QMetaObject::invokeMethod(dialog, "openSection", Q_ARG(QVariant, QString("lyric"))));
        auto *loader = visual(session.window->contentItem(), "settingsSectionLoader");
        QVERIFY(loader);
        QTRY_VERIFY(loader->property("item").value<QObject *>());
        auto *section = qobject_cast<QQuickItem *>(loader->property("item").value<QObject *>());
        QVERIFY(section);
        QTRY_VERIFY(section->implicitHeight() > 1000);
        QSet<QString> headings;
        const auto inspect = [&headings](auto &&self, QObject *object) -> void {
            const auto title = object->property("heading").toString();
            if (!title.isEmpty())
                headings.insert(title);
            for (auto *child : object->children())
                self(self, child);
        };
        inspect(inspect, section);
        for (const auto &title : {"通用", "播放页", "桌面歌词", "Mini 歌词", "显示翻译", "显示音译",
                                  "歌词过滤", "下一行预览", "主歌词字号"})
            QVERIFY2(headings.contains(QString::fromUtf8(title)), title);
        QVERIFY(!headings.contains(QStringLiteral("任务栏歌词")));
        const auto visibleItem = [&session](const QString &name) -> QQuickItem * {
            for (auto *item : visualAll(session.window->contentItem(), name))
                if (item->isVisible())
                    return item;
            return nullptr;
        };
        auto fields = visualAll(section, "lyricPlayedColor");
        QCOMPARE(fields.size(), 3);
        QList<QObject *> owners{playback.lyricAppearance(), session.windows.floating()->desktop(),
                                session.windows.floating()->mini()};
        const auto capture = qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR");
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (double scale : {1., 1.25}) {
                session.shell.resetZoom();
                while (session.shell.uiScale() < scale - .001)
                    session.shell.zoomIn();
                for (int i = 0; i < fields.size(); ++i) {
                    owners[i]->setProperty("followCoverColor", false);
                    QVERIFY(QMetaObject::invokeMethod(fields[i], "clicked"));
                    QPointer<QQuickItem> dynamic;
                    QTRY_VERIFY((dynamic = visibleItem("colorDynamicAction")) &&
                                dynamic->isVisible());
                    auto *swatch = visual(dynamic.data(), "colorDynamicSwatch");
                    QVERIFY(swatch);
                    QTRY_VERIFY(std::abs(swatch->width() - 34) < .01 &&
                                std::abs(swatch->height() - 34) < .01);
                    auto *description = visual(dynamic.data(), "colorDynamicDescription");
                    QVERIFY(description);
                    QCOMPARE(description->property("text").toString(),
                             QStringLiteral("自动使用当前封面提取色"));
                    QVERIFY(QMetaObject::invokeMethod(dynamic.data(), "clicked"));
                    QVERIFY(!owners[i]->property("followCoverColor").toBool());
                    QTest::qWait(35);
                    if (i == 0 && !capture.isEmpty())
                        QVERIFY(session.window->grabWindow().save(
                            capture + QString("/cover-picker-%1-%2.png")
                                          .arg(mode == ThemeStore::Light ? "light" : "dark")
                                          .arg(scale)));
                    QQuickItem *cancel = nullptr;
                    QTRY_VERIFY((cancel = visibleItem("colorCancelAction")));
                    QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
                    QTRY_VERIFY(!dynamic || !dynamic->isVisible());
                    QVERIFY(!owners[i]->property("followCoverColor").toBool());
                    QVERIFY(QMetaObject::invokeMethod(fields[i], "clicked"));
                    QTRY_VERIFY((dynamic = visibleItem("colorDynamicAction")) &&
                                dynamic->isVisible());
                    QVERIFY(QMetaObject::invokeMethod(dynamic.data(), "clicked"));
                    QQuickItem *confirm = nullptr;
                    QTRY_VERIFY((confirm = visibleItem("colorConfirmAction")));
                    QVERIFY(QMetaObject::invokeMethod(confirm, "clicked"));
                    QTRY_VERIFY(owners[i]->property("followCoverColor").toBool());
                    QTRY_VERIFY(!dynamic || !dynamic->isVisible());
                }
            }
        }
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(warnings.size(), 0);
    }
    void floatingPreferencesAndFilteredTimelineKeepIdentity()
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("floating.ini"), QSettings::IniFormat);
        FloatingLyricPreferences desktop(settings, true), mini(settings, false);
        QCOMPARE(desktop.alignment(), QString("both"));
        // Real QWindow logical geometry round-trips, including odd sizes at
        // fractional QT_SCALE_FACTOR. Native mouse/DPI transitions remain a
        // separate Windows device check; setPosition is not a drag simulation.
        const auto geometryArea = QGuiApplication::primaryScreen()->availableGeometry();
        const QRect expected(geometryArea.topLeft() + QPoint(16, 16), QSize(321, 147));
        settings.setValue("floating/desktop/geometry", expected);
        for (int cycle = 0; cycle < 24; ++cycle) {
            QQuickWindow window;
            window.setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
            FloatingPlayerController controller(settings, true);
            controller.setDesktopEnabled(true);
            controller.attachDesktop(&window);
            QCoreApplication::processEvents();
            QCOMPARE(window.size(), expected.size());
            for (int step = 0; step < 40; ++step) {
                window.setPosition(expected.topLeft() + QPoint(step % 9, step % 7));
                QCoreApplication::processEvents();
                QCOMPARE(window.size(), expected.size());
            }
            window.setPosition(expected.topLeft());
            controller.setLocked(true);
            controller.setLocked(false);
            QCOMPARE(window.size(), expected.size());
            controller.shutdown();
            controller.attachDesktop(nullptr);
            QCOMPARE(settings.value("floating/desktop/geometry").toRect(), expected);
        }
        QCOMPARE(mini.fontSize(), 14);
        mini.setFontSize(99);
        QCOMPARE(mini.fontSize(), 22);
        mini.setFontWeight(900);
        QCOMPARE(mini.fontWeight(), 780);
        mini.setAlignment("both");
        QCOMPARE(mini.alignment(), QString("center"));
        mini.setFollowCoverColor(true);
        mini.setPlayedColor("#FF00AA");
        FloatingLyricPreferences restored(settings, false);
        QCOMPARE(restored.playedColor(), QString("#ff00aa"));
        QVERIFY(restored.followCoverColor());
        restored.resetColors();
        QCOMPARE(restored.playedColor(), QString());
        QVERIFY(!restored.followCoverColor());
        LyricModel model(nullptr);
        QJsonArray rows;
        for (int i = 0; i < 4; ++i)
            rows.append(QJsonObject{
                {"text", i == 1 ? QStringLiteral("作词：测试") : QStringLiteral("歌词 %1").arg(i)},
                {"start_ms", i * 1000},
                {"duration_ms", 1000},
                {"words", QJsonArray{}}});
        QVERIFY(model.replace({{"lines", rows}}));
        model.setFilter(true, QStringLiteral("^作词"));
        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(model.indexAt(1500), 1);
        QCOMPARE(model.visibleIndex(1), 0);
        QCOMPARE(model.nextVisible(0), 2);
        QCOMPARE(model.startAt(2), 2000);
        QVERIFY(model.lineAt(1).value("filtered").toBool());
        model.setFilter(true, "[");
        QVERIFY(!model.lineAt(1).value("filtered").toBool());
        const QJsonArray words{
            QJsonObject{{"text", QStringLiteral("你好")}, {"start_ms", 0}, {"duration_ms", 1000}},
            QJsonObject{{"text", " world"}, {"start_ms", 1000}, {"duration_ms", 1000}}};
        QVERIFY(
            model.replace({{"lines", QJsonArray{QJsonObject{{"text", QStringLiteral("你好 world")},
                                                            {"start_ms", 0},
                                                            {"duration_ms", 2000},
                                                            {"words", words},
                                                            {"romanization", "ni hao world"}}}}}));
        const auto ruby = model.rubyAt(0);
        QCOMPARE(ruby.size(), 2);
        QCOMPARE(ruby.first().toMap().value("start_ms").toLongLong(), 0);
        QCOMPARE(ruby.last().toMap().value("start_ms").toLongLong(), 1000);
        QVERIFY(!model.verticalAt(0).isEmpty());
        const QRect area(0, 0, 1200, 800);
        QVERIFY(FloatingPlayerController::chooseUp({400, 700, 360, 64}, area));
        QVERIFY(!FloatingPlayerController::chooseUp({400, 100, 360, 64}, area));
        QVERIFY(area.contains(
            FloatingPlayerController::fitBounds({5000, 5000, 360, 64}, {area}, area, {360, 64})));
    }
    void desktopLyricsMatchSourceChromeAndHoverUnlock()
    {
        if (QGuiApplication::platformName() == "offscreen")
            QSKIP("Requires native global cursor and transparent window pixels");
        const auto originalCursor = QCursor::pos();
        const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(originalCursor); });
        Session session;
        session.theme.setMotionEnabled(false);
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.router.shutdown();
            session.commands.shutdown();
            session.windows.shutdown();
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        playback.lyricText()->setWantTranslation(false);
        playback.lyricText()->setWantRomanization(false);
        const QJsonArray rows{QJsonObject{{"text", QStringLiteral("这盛世每一天")},
                                          {"start_ms", 0},
                                          {"duration_ms", 20000},
                                          {"translation", "Peace and ordinary days"},
                                          {"romanization", "zhe sheng shi mei yi tian"},
                                          {"words", QJsonArray{}}},
                              QJsonObject{{"text", QStringLiteral("山河无恙 烟火寻常")},
                                          {"start_ms", 20000},
                                          {"duration_ms", 20000},
                                          {"words", QJsonArray{}}}};
        QVERIFY(playback.lyrics()->replace({{"lines", rows}}));
        const auto findWindow = [](const QString &name) -> QQuickWindow * {
            for (auto *candidate : QGuiApplication::topLevelWindows())
                if (candidate->objectName() == name)
                    return qobject_cast<QQuickWindow *>(candidate);
            return nullptr;
        };
        auto *floating = session.windows.floating();
        floating->setLocked(false);
        floating->setDesktopEnabled(true);
        QTRY_VERIFY(findWindow("desktopLyricWindow"));
        auto *desktop = findWindow("desktopLyricWindow");
        desktop->setGeometry(session.window->x() + 80, session.window->y() + 120, 720, 220);
        QTRY_VERIFY(QTest::qWaitForWindowExposed(desktop));
        auto *panel = visual(desktop->contentItem(), "desktopLyricPanel");
        auto *toolbar = visual(desktop->contentItem(), "desktopLyricToolbar");
        auto *viewport = visual(desktop->contentItem(), "desktopLyricViewport");
        auto *lock = visual(desktop->contentItem(), "desktopLyricLock");
        QVERIFY(panel && toolbar && viewport && lock);
        QVERIFY(!visual(desktop->contentItem(), "desktopLyricMore"));
        auto *source = visual(desktop->contentItem(), "desktopLyricSource");
        auto *settings = visual(desktop->contentItem(), "desktopLyricSettings");
        auto *translation = visual(desktop->contentItem(), "desktopLyricText-translation");
        auto *romanization = visual(desktop->contentItem(), "desktopLyricText-romanization");
        QVERIFY(source && settings && translation && romanization);
        const auto iconsReady = [](auto &&self, QQuickItem *item) -> bool {
            if (!item->isVisible())
                return true;
            if (item->objectName() == "iconImage" && item->property("status").toInt() != 1)
                return false;
            for (auto *child : item->childItems())
                if (!self(self, child))
                    return false;
            return true;
        };
        const auto capture = qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        const auto save = [&](QQuickWindow *window, const QString &name) {
            return capture.isEmpty() || window->grabWindow().save(capture + "/" + name + ".png");
        };
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            const auto suffix = mode == ThemeStore::Light ? "light" : "dark";
            Test::moveNativePointer(desktop, {-40, -40});
            QTRY_COMPARE(toolbar->opacity(), 0.);
            QTRY_COMPARE(panel->property("color").value<QColor>().alpha(), 0);
            QVERIFY(save(desktop, QString("desktop-rest-%1").arg(suffix)));
            Test::moveNativePointer(desktop, {360, 100});
            QTRY_COMPARE(toolbar->opacity(), 1.);
            QTRY_VERIFY(iconsReady(iconsReady, toolbar));
            QTRY_COMPARE(panel->property("color").value<QColor>(), QColor("#99000000"));
            QCOMPARE(toolbar->x(), 12.);
            QCOMPARE(toolbar->y(), 12.);
            QCOMPARE(viewport->x(), 24.);
            QCOMPARE(viewport->y(), 64.);
            QCOMPARE(lock->property("foreground").value<QColor>(), QColor("white"));
            QCOMPARE(lock->property("glyph").toString(), QString("lock"));
            QVERIFY(source->isVisible() && settings->isVisible());
            QVERIFY(translation->isVisible() && romanization->isVisible());
            QVERIFY(source->mapToScene({0, 0}).x() < translation->mapToScene({0, 0}).x());
            QVERIFY(romanization->mapToScene({0, 0}).x() < settings->mapToScene({0, 0}).x());
            QVERIFY(settings->mapToScene({0, 0}).x() < lock->mapToScene({0, 0}).x());
            auto *lockImage = visual(lock, "iconImage");
            QVERIFY(lockImage);
            QTRY_COMPARE(lockImage->property("status").toInt(), 1);
            Test::moveNativePointer(desktop, translation->mapToScene({16, 16}).toPoint());
            QTest::mouseClick(desktop, Qt::LeftButton, Qt::NoModifier,
                              translation->mapToScene({16, 16}).toPoint());
            QTRY_VERIFY(playback.lyricText()->wantTranslation());
            QTest::mouseClick(desktop, Qt::LeftButton, Qt::NoModifier,
                              translation->mapToScene({16, 16}).toPoint());
            QTRY_VERIFY(!playback.lyricText()->wantTranslation());
            Test::moveNativePointer(desktop, romanization->mapToScene({16, 16}).toPoint());
            QTest::mouseClick(desktop, Qt::LeftButton, Qt::NoModifier,
                              romanization->mapToScene({16, 16}).toPoint());
            QTRY_VERIFY(playback.lyricText()->wantRomanization());
            QTest::mouseClick(desktop, Qt::LeftButton, Qt::NoModifier,
                              romanization->mapToScene({16, 16}).toPoint());
            QTRY_VERIFY(!playback.lyricText()->wantRomanization());
            Test::moveNativePointer(desktop, {360, 100});
            auto image = desktop->grabWindow();
            QVERIFY(!image.isNull());
            const auto lockBounds = lock->mapRectToScene({0, 0, lock->width(), lock->height()});
            const auto ratio = image.width() / qreal(desktop->width());
            const auto lockPixels = image.copy(
                QRectF(lockBounds.topLeft() * ratio, lockBounds.size() * ratio).toRect());
            int lockInk = 0;
            for (int y = 0; y < lockPixels.height(); ++y)
                for (int x = 0; x < lockPixels.width(); ++x) {
                    const auto ink = lockPixels.pixelColor(x, y);
                    lockInk += ink.alpha() > 220 && ink.red() > 220 && ink.green() > 220 &&
                               ink.blue() > 220;
                }
            QVERIFY(lockInk > 20);
            auto background = image.pixelColor(image.width() / 2, image.height() - 12);
            QVERIFY2(background.red() < 3 && background.green() < 3 && background.blue() < 3,
                     qPrintable(background.name(QColor::HexArgb)));
            QVERIFY(qAbs(background.alpha() - 153) <= 2);
            QVERIFY(save(desktop, QString("desktop-hover-%1").arg(suffix)));
#ifdef Q_OS_MACOS
            QVERIFY(!nativeFloatingAppearance(desktop).value("titled").toBool());
            QVERIFY(!nativeFloatingAppearance(desktop).value("opaque").toBool());
#endif
            Test::moveNativePointer(desktop, {-40, -40});
            floating->setLocked(true);
            QTRY_VERIFY(findWindow("desktopLyricUnlockWindow"));
            QPointer<QQuickWindow> unlock = findWindow("desktopLyricUnlockWindow");
            QTRY_VERIFY(!unlock->isVisible());
            QVERIFY(!floating->unlockVisible());
            QVERIFY(desktop->flags().testFlag(Qt::WindowTransparentForInput));
            Test::moveNativePointer(desktop, {360, 100});
            QTRY_VERIFY(unlock->isVisible());
            QTRY_VERIFY(floating->unlockVisible());
            QCOMPARE(toolbar->opacity(), 0.);
            QCOMPARE(panel->property("color").value<QColor>().alpha(), 0);
            QTRY_COMPARE(unlock->geometry().topLeft(),
                         desktop->mapToGlobal(lock->mapToScene({0, 0}).toPoint()));
            QCOMPARE(unlock->size(), lock->size().toSize());
            QVERIFY(!unlock->flags().testFlag(Qt::WindowTransparentForInput));
            auto *button = visual(unlock->contentItem(), "desktopLyricUnlock");
            QVERIFY(button);
            QCOMPARE(button->property("foreground").value<QColor>(), QColor("white"));
            image = unlock->grabWindow();
            background = image.pixelColor(image.width() / 2, image.height() - 4);
            QVERIFY(background.red() < 3 && background.green() < 3 && background.blue() < 3);
            QVERIFY(qAbs(background.alpha() - 115) <= 2);
            int whitePixels = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const auto ink = image.pixelColor(x, y);
                    whitePixels += ink.alpha() > 220 && ink.red() > 220 && ink.green() > 220 &&
                                   ink.blue() > 220;
                }
            QVERIFY(whitePixels > 20);
            QVERIFY(save(unlock, QString("unlock-%1").arg(suffix)));
            QVERIFY(save(desktop, QString("desktop-locked-%1").arg(suffix)));
            Test::moveNativePointer(desktop, {-40, -40});
            QTRY_VERIFY(!unlock->isVisible());
            // Revealing/hiding must reuse the tiny window, not remount the lyric viewport.
            QCOMPARE(unlock.data(), findWindow("desktopLyricUnlockWindow"));
            Test::moveNativePointer(desktop, {360, 100});
            QTRY_VERIFY(unlock->isVisible());
            floating->desktop()->setShowUnlockButton(false);
            QTRY_VERIFY(unlock.isNull());
            QVERIFY(!floating->unlockAvailable());
            floating->desktop()->setShowUnlockButton(true);
            QTRY_VERIFY(findWindow("desktopLyricUnlockWindow"));
            unlock = findWindow("desktopLyricUnlockWindow");
            QTRY_VERIFY(unlock->isVisible());
            desktop->hide();
            QTRY_VERIFY(!unlock->isVisible());
            desktop->show();
            QTRY_VERIFY(unlock->isVisible());
            // The lock and escape control share geometry, also after a layout switch.
            floating->desktop()->setLayout("vertical");
            desktop->resize(240, 600);
            Test::moveNativePointer(desktop, {80, 100});
            QTRY_VERIFY(unlock->isVisible());
            QTRY_COMPARE(unlock->geometry().topLeft(),
                         desktop->mapToGlobal(lock->mapToScene({0, 0}).toPoint()));
            QVERIFY(viewport->x() + viewport->width() < toolbar->x());
            for (const auto &name :
                 {QString("desktopLyricOffsets"), QString("desktopLyricTransport"),
                  QString("desktopLyricWindowActions")}) {
                auto *group = visual(desktop->contentItem(), name);
                QVERIFY(group);
                QTRY_VERIFY(qAbs(group->mapToScene({group->width() / 2, 0}).x() -
                                 (toolbar->x() + toolbar->width() / 2)) < 1);
            }
            QVERIFY(save(desktop, QString("desktop-vertical-locked-%1").arg(suffix)));
            floating->setLocked(false);
            QTRY_VERIFY(unlock.isNull());
            Test::moveNativePointer(desktop, {80, 100});
            QTRY_COMPARE(toolbar->opacity(), 1.);
            QTRY_VERIFY(iconsReady(iconsReady, toolbar));
            QVERIFY(save(desktop, QString("desktop-vertical-hover-%1").arg(suffix)));
            floating->desktop()->setLayout("horizontal");
            desktop->resize(720, 220);
            Test::moveNativePointer(desktop, {360, 100});
            floating->setLocked(true);
            QTRY_VERIFY(findWindow("desktopLyricUnlockWindow"));
            unlock = findWindow("desktopLyricUnlockWindow");
            QTRY_VERIFY(unlock->isVisible());
            Test::moveNativePointer(unlock, {16, 16});
            QTest::mouseClick(unlock, Qt::LeftButton, Qt::NoModifier, {16, 16});
            QTRY_VERIFY(!floating->locked());
            QTRY_VERIFY(unlock.isNull());
            QVERIFY(!desktop->flags().testFlag(Qt::WindowTransparentForInput));
            QTRY_COMPARE(toolbar->opacity(), 1.);
        }
        floating->setLocked(true);
        floating->setDesktopEnabled(false);
        QTRY_VERIFY(!findWindow("desktopLyricUnlockWindow"));
        QTRY_VERIFY(!findWindow("desktopLyricWindow"));
        QVERIFY(!floating->unlockAvailable());
        QVERIFY(!floating->unlockVisible());
    }
    void nativeFloatingWindowsShareStateAndReleaseAcrossCycles()
    {
        Session session;
        session.theme.setMotionEnabled(false);
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.router.shutdown();
            session.commands.shutdown();
            session.windows.shutdown();
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QJsonArray rows;
        const bool measure = qEnvironmentVariableIntValue("ECHO_FLOATING_PERF");
        const auto initialRss = measure ? floatingRssKiB() : 0;
        const int rowCount = measure ? 10000 : 150;
        for (int i = 0; i < rowCount; ++i)
            rows.append(QJsonObject{{"text", QStringLiteral("夜色温柔 让音乐留在身边 %1").arg(i)},
                                    {"start_ms", i * 1000},
                                    {"duration_ms", 1000},
                                    {"words", QJsonArray{}},
                                    {"translation", "Stay close to the music"},
                                    {"romanization", "ye se wen rou"}});
        QVERIFY(playback.lyrics()->replace({{"lines", rows}}));
        const auto findWindow = [](const QString &name) -> QQuickWindow * {
            for (auto *candidate : QGuiApplication::topLevelWindows())
                if (candidate->objectName() == name)
                    return qobject_cast<QQuickWindow *>(candidate);
            return nullptr;
        };
        auto *floating = session.windows.floating();
        const auto capture = qEnvironmentVariable("ECHO_FLOATING_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (int cycle = 0; cycle < (measure ? 8 : 3); ++cycle) {
            floating->setDesktopEnabled(true);
            QTRY_VERIFY(findWindow("desktopLyricWindow"));
            QPointer<QQuickWindow> desktop = findWindow("desktopLyricWindow");
            QTRY_VERIFY(desktop->isVisible());
            QVERIFY(!desktop->transientParent());
            auto *viewport = visual(desktop->contentItem(), "desktopLyricViewport");
            QVERIFY(viewport);
            if (cycle == 0) {
                playback.lyricText()->setWantTranslation(false);
                playback.lyricText()->setWantRomanization(false);
                QVERIFY(!visual(desktop->contentItem(), "desktopLyricMore"));
                for (const auto &name :
                     {QString("desktopLyricSource"), QString("desktopLyricSettings"),
                      QString("desktopLyricOffset--1"), QString("desktopLyricOffset-1")}) {
                    auto *button = visual(desktop->contentItem(), name);
                    QVERIFY(button && button->isVisible());
                    auto *icon = visual(button, "iconImage");
                    QVERIFY(icon);
                    QTRY_COMPARE(icon->property("status").toInt(), 1);
                }
                auto *translation = visual(desktop->contentItem(), "desktopLyricText-translation");
                auto *romanization =
                    visual(desktop->contentItem(), "desktopLyricText-romanization");
                QVERIFY(translation && translation->isVisible());
                QVERIFY(romanization && romanization->isVisible());
                QTest::mouseClick(desktop, Qt::LeftButton, Qt::NoModifier,
                                  translation->mapToScene({16, 16}).toPoint());
                QTRY_VERIFY(playback.lyricText()->wantTranslation());
                QTest::mouseClick(desktop, Qt::LeftButton, Qt::NoModifier,
                                  translation->mapToScene({16, 16}).toPoint());
                QTRY_VERIFY(!playback.lyricText()->wantTranslation());
                QTest::mouseClick(desktop, Qt::LeftButton, Qt::NoModifier,
                                  romanization->mapToScene({16, 16}).toPoint());
                QTRY_VERIFY(playback.lyricText()->wantRomanization());
                QTest::mouseClick(desktop, Qt::LeftButton, Qt::NoModifier,
                                  romanization->mapToScene({16, 16}).toPoint());
                QTRY_VERIFY(!playback.lyricText()->wantRomanization());
                auto *transport = visual(desktop->contentItem(), "desktopLyricTransport");
                auto *actions = visual(desktop->contentItem(), "desktopLyricWindowActions");
                QVERIFY(transport && actions);
                QTRY_VERIFY(transport->width() > 90 && actions->width() > 90);
                QTRY_VERIFY(qAbs(transport->mapToScene({transport->width() / 2, 0}).x() -
                                 desktop->width() / 2.) < 1);
                QTRY_VERIFY(qAbs(actions->mapToScene({actions->width(), 0}).x() -
                                 (desktop->width() - 12)) < 1);
                auto *title = visual(desktop->contentItem(), "desktopLyricTitle");
                QVERIFY(title);
                title->setProperty(
                    "text", QStringLiteral(
                                "这是很长的曲目信息 Long title does not shift playback controls"));
                for (const int width : {720, 420, 360, 280, 240}) {
                    desktop->resize(width, 200);
                    QTRY_VERIFY(qAbs(transport->mapToScene({transport->width() / 2, 0}).x() -
                                     width / 2.) < 1);
                    QTRY_VERIFY(actions->mapToScene({0, 0}).x() >=
                                transport->mapToScene({transport->width(), 0}).x() + 6);
                }
                desktop->resize(720, 200);
                for (const auto size : {QSize(720, 140), QSize(720, 220), QSize(720, 360)}) {
                    desktop->resize(size);
                    QTRY_COMPARE(viewport->property("fontSize").toInt(), size.height() == 140 ? 20
                                                                         : size.height() == 220
                                                                             ? 48
                                                                             : 96);
                    QCOMPARE(desktop->size(), size);
                }
                playback.lyricText()->setWantTranslation(true);
                playback.lyricText()->setWantRomanization(true);
                QTRY_COMPARE(viewport->property("fontSize").toInt(), 67);
                floating->desktop()->setLayout("vertical");
                desktop->resize(320, 600);
                QTRY_COMPARE(viewport->property("fontSize").toInt(), 42);
                desktop->resize(520, 600);
                QTRY_COMPARE(viewport->property("fontSize").toInt(), 64);
                floating->desktop()->setLayout("horizontal");
                desktop->resize(80, 100);
                auto *close = visual(desktop->contentItem(), "desktopLyricClose");
                QVERIFY(close);
                QTRY_VERIFY(close->mapToScene({0, 0}).x() >= 6 &&
                            close->mapToScene({close->width(), 0}).x() <= 74);
                desktop->resize(720, 200);
                QCoreApplication::processEvents();
                // Font/content changes must never write a new native size.
                QCOMPARE(desktop->size(), QSize(720, 200));
            }
            viewport->setProperty("renderTime", 1500);
            QTRY_COMPARE(viewport->property("currentIndex").toInt(), 1);
            QTest::qWait(80);
            if (!capture.isEmpty() && cycle == 0) {
                Test::moveNativePointer(desktop, {180, 22});
                auto *toolbar = visual(desktop->contentItem(), "desktopLyricToolbar");
                QVERIFY(toolbar);
                QTRY_VERIFY(toolbar->opacity() > .95);
                QVERIFY(desktop->grabWindow().save(capture + "/desktop-light.png"));
            }
            playback.lyricText()->setWantRomanization(true);
            playback.lyricText()->setRomanizationAsRuby(true);
            QTest::qWait(50);
            if (!capture.isEmpty() && cycle == 0)
                QVERIFY(desktop->grabWindow().save(capture + "/desktop-ruby.png"));
            floating->desktop()->setLayout("vertical");
            desktop->resize(200, 600);
            QTest::qWait(80);
            if (!capture.isEmpty() && cycle == 0)
                QVERIFY(desktop->grabWindow().save(capture + "/desktop-vertical.png"));
            floating->desktop()->setLayout("horizontal");
            desktop->resize(720, 200);
            floating->setLocked(true);
            QTRY_VERIFY(desktop->flags().testFlag(Qt::WindowTransparentForInput));
#ifdef Q_OS_MACOS
            if (QGuiApplication::platformName() == "cocoa") {
                QTRY_VERIFY(nativeFloatingAppearance(desktop).value("ignoresMouseEvents").toBool());
                QVERIFY(nativeFloatingAppearance(desktop).value("allSpaces").toBool());
                QVERIFY(nativeFloatingAppearance(desktop).value("fullscreenAuxiliary").toBool());
                QVERIFY(!nativeFloatingAppearance(desktop).value("hidesOnDeactivate").toBool());
            }
#endif
            QTRY_VERIFY(findWindow("desktopLyricUnlockWindow"));
            floating->setLocked(false);
            QTRY_VERIFY(!desktop->flags().testFlag(Qt::WindowTransparentForInput));
#ifdef Q_OS_MACOS
            if (QGuiApplication::platformName() == "cocoa") {
                QTRY_VERIFY(
                    !nativeFloatingAppearance(desktop).value("ignoresMouseEvents").toBool());
            }
#endif
            QTRY_VERIFY(!findWindow("desktopLyricUnlockWindow"));
            floating->setDesktopEnabled(false);
            QTRY_VERIFY(desktop.isNull());
            floating->toggleMini();
            QTRY_VERIFY(findWindow("miniPlayerWindow"));
            QPointer<QQuickWindow> mini = findWindow("miniPlayerWindow");
            QTRY_VERIFY(!session.window->isVisible());
            QCOMPARE(mini->size(), QSize(360, 64));
            auto *controls = visual(mini->contentItem(), "miniPlayerControls");
            QVERIFY(controls);
            const auto area = mini->screen()->availableGeometry();
            mini->setPosition(area.x() + 80, area.y() + 80);
            if (cycle == 0) {
                QVERIFY(Test::prepareNativeInput(mini));
                auto *pin = visual(mini->contentItem(), "miniPlayerPin");
                QVERIFY(pin);
                Test::moveNativePointer(mini, {-30, -30});
                QTRY_VERIFY(!mini->property("showControls").toBool());
                QTRY_VERIFY(pin->isVisible() && pin->opacity() == 1 && pin->width() >= 24 &&
                            pin->height() >= 30);
                QCOMPARE(pin->property("checked").toBool(), floating->mini()->alwaysOnTop());
                if (!capture.isEmpty())
                    QVERIFY(mini->grabWindow().save(capture + "/mini-pin-rest.png"));
                const auto clickPin = [&] {
                    const auto center =
                        pin->mapToScene({pin->width() / 2, pin->height() / 2}).toPoint();
                    QTest::mouseClick(mini, Qt::LeftButton, Qt::NoModifier, center);
                };
                const bool initialPin = floating->mini()->alwaysOnTop();
                clickPin();
                QTRY_COMPARE(floating->mini()->alwaysOnTop(), !initialPin);
                QTRY_COMPARE(pin->property("checked").toBool(), !initialPin);
                QTRY_COMPARE(mini->flags().testFlag(Qt::WindowStaysOnTopHint), !initialPin);
                clickPin();
                QTRY_COMPARE(floating->mini()->alwaysOnTop(), initialPin);
                QTRY_COMPARE(pin->property("checked").toBool(), initialPin);
                QTRY_COMPARE(mini->flags().testFlag(Qt::WindowStaysOnTopHint), initialPin);
            }
            const auto before = mini->position() + controls->mapToScene({0, 0}).toPoint();
            floating->setPanel("lyric");
            QTRY_COMPARE(mini->height(), 360);
            QTRY_VERIFY(!floating->expandsUp());
            QTRY_VERIFY((mini->position() + controls->mapToScene({0, 0}).toPoint() - before)
                            .manhattanLength() <= 1);
            QTest::qWait(80);
            const auto countInk = [](auto &&self, QQuickItem *item) -> int {
                int count = item->inherits("EchoMusic::LyricLine") ? 1 : 0;
                for (auto *child : item->childItems())
                    count += self(self, child);
                return count;
            };
            const int mounted = countInk(countInk, mini->contentItem());
            QVERIFY(mounted > 0 && mounted < 100);
            if (measure)
                qInfo() << "floating-mini-cycle=" << cycle << "model-rows=" << rowCount
                        << "mounted-lyric-renderers=" << mounted << "rss-kib=" << floatingRssKiB();
            if (!capture.isEmpty() && cycle == 0)
                QVERIFY(mini->grabWindow().save(capture + "/mini-lyrics.png"));
            floating->setPanel("queue");
            QTRY_VERIFY(visual(mini->contentItem(), "miniPlayerQueue")->isVisible());
            floating->setPanel("");
            QTRY_COMPARE(mini->height(), 64);
            mini->setPosition(area.x() + 80, area.bottom() - 90);
            const auto lower = mini->position() + controls->mapToScene({0, 0}).toPoint();
            floating->setPanel("lyric");
            QTRY_VERIFY(floating->expandsUp());
            QTRY_VERIFY((mini->position() + controls->mapToScene({0, 0}).toPoint() - lower)
                            .manhattanLength() <= 1);
            floating->showMain();
            QTRY_VERIFY(mini.isNull());
            QTRY_VERIFY(session.window->isVisible());
        }
        QTest::qWait(150);
        if (measure)
            qInfo() << "floating-closed model-rows=" << rowCount << "initial-rss-kib=" << initialRss
                    << "closed-rss-kib=" << floatingRssKiB();
        QCOMPARE(warnings.size(), 0);
    }
    void audibleTransitionPublishesOneToastThroughShell()
    {
        Session session;
        const auto path = session.directory.filePath("transition-a.wav");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        constexpr quint32 bytes = 48000 * 10 * 2;
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + bytes);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(48000) << quint32(96000)
               << quint16(2) << quint16(16);
        stream.writeRawData("data", 4);
        stream << bytes;
        for (int i = 0; i < 48000 * 10; ++i)
            stream << qint16(std::sin(i * 2. * 3.141592653589793 * 220 / 48000) * 1000);
        file.close();
        const auto second = session.directory.filePath("transition-b.wav");
        QVERIFY(QFile::copy(path, second));
        PlaybackTracks tracks;
        for (const auto &source : {path, second}) {
            PlaybackTrack track;
            track.id = source;
            track.localPath = source;
            track.title = "Native transition";
            track.durationMs = 10000;
            tracks.append(track);
        }
        PlaybackService playback(session.api, session.preferences);
        playback.setVolume(0);
        QVERIFY(session.create(&playback));
        session.theme.setMotionEnabled(false);
        auto *toasts = session.commands.toasts();
        playback.transition()->setMode("gapless");
        QVERIFY(!toasts->visible());
        playback.playTracks(tracks, 0, "Toast regression", "native-toast");
        QTRY_VERIFY_WITH_TIMEOUT(playback.playing(), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(
            TrackTransitionControllerTestAccess::prepared(*playback.transition()), 15000);
        QVERIFY(!toasts->visible());
        playback.next();
        QTRY_COMPARE_WITH_TIMEOUT(playback.currentTrack().key(), tracks[1].key(), 15000);
        QTRY_VERIFY(toasts->visible());
        QCOMPARE(toasts->message(), QString("无缝"));
        QCOMPARE(toasts->tone(), ToastService::Info);
        QCOMPARE(toasts->repetitions(), 1);
        auto *message = visual(session.window->contentItem(), "toastMessage");
        auto *card = visual(session.window->contentItem(), "toastCard");
        QVERIFY(message && card);
        QTRY_VERIFY(card->isVisible());
        QCOMPARE(message->property("text").toString(), QString("无缝"));
        QTRY_VERIFY_WITH_TIMEOUT(!toasts->visible(), 5000);
        session.root.reset();
        session.window = nullptr;
        QSignalSpy closed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
    }
    void nativePlaybackSurvivesRepeatedAppearanceUpdates()
    {
#ifndef Q_OS_MACOS
        QSKIP("macOS appearance scheduling regression");
#endif
        Session session;
        const auto path = session.directory.filePath("appearance-silence.wav");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        constexpr quint32 bytes = 48000 * 4 * 30;
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + bytes);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(2) << quint32(48000) << quint32(192000)
               << quint16(4) << quint16(16);
        stream.writeRawData("data", 4);
        stream << bytes;
        const QByteArray silence(bytes, '\0');
        QCOMPARE(stream.writeRawData(silence.constData(), silence.size()), silence.size());
        file.close();
        // Exercise the real FFmpeg/filter/CoreAudio bridge in the same process
        // as production QML, without account writes or a simulated audio clock.
        AudioEngine audio;
        QSignalSpy events(&audio, &AudioEngine::eventReceived),
            closed(&audio, &AudioEngine::closed);
        const auto contains = [&](const QString &kind) {
            for (const auto &event : events)
                if (event.front().toJsonObject().value("event").toString() == kind)
                    return true;
            return false;
        };
        const auto position = [&] {
            for (auto it = events.crbegin(); it != events.crend(); ++it) {
                const auto event = it->front().toJsonObject();
                if (event.value("event").toString() == "time-update")
                    return event.value("time").toDouble();
            }
            return 0.0;
        };
        const auto underruns = [&] {
            for (auto it = events.crbegin(); it != events.crend(); ++it) {
                const auto event = it->front().toJsonObject();
                if (event.value("event").toString() == "audio-output-stats")
                    return event.value("output_stats").toObject().value("underruns").toDouble(-1);
            }
            return -1.0;
        };
        QVERIFY(session.create());
        QVERIFY(Test::prepareNativeInput(session.window));
        QTRY_VERIFY_WITH_TIMEOUT(contains("ready"), 15000);
        audio.load(path, 91);
        QTRY_VERIFY_WITH_TIMEOUT(contains("file-loaded"), 15000);
        audio.command(AudioEngine::Play);
        QTRY_VERIFY_WITH_TIMEOUT(position() > 0.5 && underruns() >= 0, 5000);
        const auto initialPosition = position(), initialUnderruns = underruns();
        QElapsedTimer elapsed;
        elapsed.start();
        for (int i = 0; i < 32; ++i) {
            session.theme.setMode(i % 2 ? ThemeStore::Light : ThemeStore::Dark);
            session.window->update();
            QTest::qWait(100);
        }
        QTRY_VERIFY_WITH_TIMEOUT(position() > initialPosition + 2.5, 5000);
        QCOMPARE(underruns(), initialUnderruns);
        qInfo() << "32 production appearance updates; audio seconds advanced"
                << position() - initialPosition << "elapsed ms" << elapsed.elapsed()
                << "output underruns before/after" << initialUnderruns << underruns();
        audio.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
    }
    void productionHeaderWindowActionsStayAtRightEdge()
    {
        Session session;
        session.preferences.setValue("playback/volume", 0);
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.shell.setMotionEnabled(false);
        session.window->setMinimumWidth(480);
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *header = visual(session.window->contentItem(), "shellTitleBar");
        auto *actions = visual(header, "titlebarWindowActions");
        auto *more = visual(header, "titlebarMoreAction");
        // 弹层按需创建：经真实入口所有者触发后再取实例，关闭后整体释放。
        auto *pageOwner = session.root->findChild<QObject *>("playbackPageOwner");
        QVERIFY(header && actions && more && pageOwner);
        QPointer<QObject> page;
        auto *fullscreen = visual(actions, "windowFullscreenAction");
        auto *caption = visual(actions, "windowCaptionControls");
        auto *close = visual(caption, "windowCloseAction");
        QVERIFY(fullscreen && caption && close);
        QCOMPARE(actions->property("nativeCaption").toBool(), session.windows.macOS());
        QCOMPARE(caption->isVisible(), !session.windows.macOS());
        QCOMPARE(fullscreen->isVisible(), !session.windows.macOS());
        // Exercise both presentation policies on the host platform. Window manager
        // behavior is still checked separately on actual Windows/X11/Wayland.
        const auto capture = qEnvironmentVariable("ECHO_HEADER_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool nativeCaption : {true, false}) {
                actions->setProperty("nativeCaption", nativeCaption);
                for (int zoom : {0, 5}) {
                    session.shell.resetZoom();
                    for (int step = 0; step < zoom; ++step)
                        session.shell.zoomIn();
                    for (int width : {500, 1150, 1600}) {
                        session.window->resize(width, 750);
                        QTRY_VERIFY(qAbs(actions->width() - actions->implicitWidth()) < .6);
                        QTRY_VERIFY(qAbs(actions->mapToItem(header, {actions->width(), 0}).x() -
                                         (header->width() - 12)) < .6);
                        if (more->isVisible())
                            QTRY_VERIFY(qAbs(actions->x() - (more->x() + more->width()) - 12) < .6);
                        if (!nativeCaption) {
                            QVERIFY(caption->isVisible());
                            QTRY_COMPARE(close->size(), QSizeF(34, 34));
                            const auto sceneRect = close->mapRectToScene(close->boundingRect());
                            QVERIFY(qAbs(sceneRect.width() - 34 * session.shell.uiScale()) < .6);
                            QVERIFY(qAbs(sceneRect.height() - 34 * session.shell.uiScale()) < .6);
                            QTRY_VERIFY(qAbs(close->mapToItem(header, {34, 17}).x() -
                                             (header->width() - 12)) < .6);
                            QTRY_VERIFY(qAbs(close->mapToItem(header, {17, 17}).y() -
                                             header->height() / 2) < .6);
                        }
                    }
                }
                session.shell.resetZoom();
                session.window->resize(1150, 750);
                QVERIFY(QMetaObject::invokeMethod(pageOwner, "open",
                                                  Q_ARG(QVariant, QVariant(QVariantMap{}))));
                page = pageOwner->property("item").value<QObject *>();
                QTRY_VERIFY(page && page->property("opened").toBool());
                QQuickItem *overlayActions = nullptr;
                QTRY_VERIFY((overlayActions =
                                 visual(session.window->contentItem(), "playbackWindowActions")));
                overlayActions->setProperty("nativeCaption", nativeCaption);
                auto *loader = visual(session.window->contentItem(), "playbackWindowActionsLoader");
                QVERIFY(loader);
                QTRY_COMPARE(overlayActions->width(), nativeCaption ? 0. : 148.);
                QVERIFY(!overlayActions->property("showMiniPlayer").toBool());
                QVERIFY(!visual(overlayActions, "windowMiniPlayerAction")->isVisible());
                QVERIFY(actions->property("showMiniPlayer").toBool());
                auto *overlayClose = visual(overlayActions, "windowCloseAction");
                QVERIFY(overlayClose);
                QCOMPARE(overlayClose->isVisible(), !nativeCaption);
                QCOMPARE(overlayClose->property("colors"), page->property("colors"));
                for (int zoom : {0, 5}) {
                    session.shell.resetZoom();
                    for (int step = 0; step < zoom; ++step)
                        session.shell.zoomIn();
                    for (int width : {500, 1150, 1600}) {
                        session.window->resize(width, 750);
                        QTRY_VERIFY(qAbs(loader->mapToScene({loader->width(), 0}).x() -
                                         (session.window->width() - 12)) < .6);
                        if (!nativeCaption) {
                            for (const auto *name :
                                 {"windowFullscreenAction", "windowMinimizeAction",
                                  "windowMaximizeAction", "windowCloseAction"}) {
                                auto *button = visual(overlayActions, name);
                                QVERIFY(button && button->isVisible());
                                const auto rect = button->mapRectToScene(button->boundingRect());
                                QVERIFY(qAbs(rect.width() - 34) < .6);
                                QVERIFY(qAbs(rect.height() - 34) < .6);
                                QVERIFY(qAbs(rect.center().y() - 23) < .6);
                            }
                        }
                    }
                    if (!capture.isEmpty()) {
                        session.window->resize(1150, 750);
                        QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                        session.window->update();
                        QTRY_VERIFY(!frames.isEmpty());
                        QVERIFY(session.window->grabWindow().save(
                            capture + QString("/playback-%1-%2-%3.png")
                                          .arg(mode == ThemeStore::Dark ? "dark" : "light",
                                               nativeCaption ? "native" : "custom",
                                               zoom ? "125" : "100")));
                    }
                }
                session.shell.resetZoom();
                QMetaObject::invokeMethod(page, "close");
                QTRY_VERIFY(page.isNull());
                if (!capture.isEmpty()) {
                    QTest::qWait(150);
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/main-%1-%2.png")
                                      .arg(mode == ThemeStore::Dark ? "dark" : "light",
                                           nativeCaption ? "native" : "custom")));
                }
            }
        }
        actions->setProperty("nativeCaption", false);
        session.shell.setShowFullscreenButton(false);
        QTRY_VERIFY(!fullscreen->isVisible());
        QVERIFY(caption->isVisible());
        QCOMPARE(session.preferences.value("interface/showFullscreenButton").toBool(), false);
        ShellSettings reloaded(session.preferences);
        QVERIFY(!reloaded.showFullscreenButton());
        session.shell.setShowFullscreenButton(true);
        QTRY_VERIFY(fullscreen->isVisible());
        session.window->resize(1150, 750);
        auto *blockedInput = actions->property("background").value<QQuickItem *>();
        auto *mini = visual(actions, "windowMiniPlayerAction");
        // Mini 已是可用入口；保留原断言意图——按钮间的间隔按压必须被
        // WindowActionGroup 背景吞掉，不得穿透到窗口拖动层。
        QVERIFY(blockedInput && mini);
        QTest::qWait(100);
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier,
                          actions->mapToScene({36, 17}).toPoint());
        QVERIFY(blockedInput->property("pressed").toBool());
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier,
                            actions->mapToScene({36, 17}).toPoint());
        QTest::mouseDClick(session.window, Qt::LeftButton, Qt::NoModifier,
                           actions->mapToScene({36, 17}).toPoint());
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier,
                            actions->mapToScene({36, 17}).toPoint());
        QVERIFY(!session.windows.maximized());
        auto click = [&](QQuickItem *button) {
            QVERIFY(Test::prepareNativeInput(session.window));
            QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
            session.window->update();
            QTRY_VERIFY(!frames.isEmpty());
            const bool togglesFullscreen = button->objectName() == "windowFullscreenAction";
            const bool expectedFullscreen = !session.windows.fullscreen();
#ifdef Q_OS_MACOS
            // Observe before clicking: Qt state changes before AppKit's Spaces
            // transition finishes, so the inverse action must wait for did-exit
            // or did-enter rather than racing a still-running native transition.
            if (QGuiApplication::platformName() == "cocoa")
                nativeWindowAppearance(session.window);
#endif
            QSignalSpy clicked(button, SIGNAL(clicked()));
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                              button->mapToScene({17, 17}).toPoint());
            QCOMPARE(clicked.count(), 1);
            if (togglesFullscreen) {
                QTRY_COMPARE(session.windows.fullscreen(), expectedFullscreen);
#ifdef Q_OS_MACOS
                if (QGuiApplication::platformName() == "cocoa")
                    QTRY_COMPARE_WITH_TIMEOUT(
                        nativeWindowAppearance(session.window)
                            .value("fullscreenSettled")
                            .toString(),
                        expectedFullscreen ? QString("full") : QString("normal"), 10000);
#endif
            }
        };
        auto *maximize = visual(actions, "windowMaximizeAction");
        QVERIFY(maximize);
        click(maximize);
        QTRY_VERIFY(session.windows.maximized());
        QTRY_COMPARE(maximize->property("glyph").toString(), QString("copy"));
        click(maximize);
        QTRY_VERIFY(!session.windows.maximized());
        QTRY_COMPARE(maximize->property("glyph").toString(), QString("square"));
        click(fullscreen);
        QTRY_VERIFY(session.windows.fullscreen());
        QTRY_VERIFY(!caption->isVisible());
        QVERIFY(fullscreen->isVisible());
        click(fullscreen);
        QTRY_VERIFY(!session.windows.fullscreen());
        QTRY_VERIFY(caption->isVisible());
        click(maximize);
        QTRY_VERIFY(session.windows.maximized());
        click(fullscreen);
        QTRY_VERIFY(session.windows.fullscreen());
        click(fullscreen);
        QTRY_VERIFY(session.windows.maximized());
        click(maximize);
        QTRY_VERIFY(!session.windows.maximized());
        QVERIFY(
            QMetaObject::invokeMethod(pageOwner, "open", Q_ARG(QVariant, QVariant(QVariantMap{}))));
        page = pageOwner->property("item").value<QObject *>();
        QTRY_VERIFY(page && page->property("opened").toBool());
        auto *overlayActions = visual(session.window->contentItem(), "playbackWindowActions");
        QVERIFY(overlayActions);
        overlayActions->setProperty("nativeCaption", false);
        auto *overlayFullscreen = visual(overlayActions, "windowFullscreenAction");
        auto *overlayCaption = visual(overlayActions, "windowCaptionControls");
        QVERIFY(overlayFullscreen && overlayCaption);
        click(overlayFullscreen);
        QTRY_VERIFY(session.windows.fullscreen());
        QTRY_VERIFY(!overlayCaption->isVisible());
        click(overlayFullscreen);
        QTRY_VERIFY(!session.windows.fullscreen());
        QTRY_VERIFY(overlayCaption->isVisible());
        QMetaObject::invokeMethod(page, "close");
        QTRY_VERIFY(page.isNull());
        if (!session.windows.macOS()) {
            QTest::keyClick(session.window, Qt::Key_F11);
            QTRY_VERIFY(session.windows.fullscreen());
            QTest::keyClick(session.window, Qt::Key_F11);
            QTRY_VERIFY(!session.windows.fullscreen());
        }
        auto *minimize = visual(actions, "windowMinimizeAction");
        QVERIFY(minimize);
        click(minimize);
        QTRY_COMPARE(session.window->windowState(), Qt::WindowMinimized);
        session.windows.showWindow();
        QVERIFY(Test::prepareNativeInput(session.window));
        QTRY_COMPARE(session.window->windowState(), Qt::WindowNoState);
        QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
        session.window->update();
        QTRY_VERIFY(!frames.isEmpty());
        QTest::mouseMove(session.window, close->mapToScene({17, 17}).toPoint());
        QTRY_VERIFY(close->property("hovered").toBool());
        QTRY_COMPARE(close->property("foreground").value<QColor>(), QColor("#ffffff"));
        auto *closeBackground = close->property("background").value<QQuickItem *>();
        QVERIFY(closeBackground);
        QCOMPARE(closeBackground->property("color").value<QColor>(), QColor("#c42b1c"));
        session.windows.setCloseToTray(false);
        QSignalSpy closeClicked(close, SIGNAL(clicked()));
        click(close);
        QTRY_COMPARE(closeClicked.size(), 1);
        QTRY_VERIFY(!session.window->isVisible());
        QCOMPARE(warnings.size(), 0);
    }
    void productionExperimentalIdentitySettings_data()
    {
        productionNetworkSettingsDraftApplyResetAndTheme_data();
    }
    void productionExperimentalIdentitySettings()
    {
        QFETCH(bool, dark);
        QFETCH(qreal, scale);
        Session session;
        QVERIFY(session.api.setNetworkTransport(1)); // Keep synthetic diagnostics offline.
        RelaunchService relaunch({}, {}, false);
        ExperimentalSettingsService experimental(session.api, session.accounts.account,
                                                 session.accounts.saved, session.preferences,
                                                 relaunch);
        QVERIFY(session.accounts.account.adoptLoginAuthorization(QByteArrayLiteral(
            "{\"user_id\":42,\"token\":\"synthetic-visible-token\",\"refresh_credential\":"
            "\"synthetic-visible-t1\",\"nickname\":\"实验室测试账号\"}")));
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(nullptr, nullptr, nullptr, &relaunch, nullptr, &experimental));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        session.theme.setFloatingFrosted(false);
        session.theme.setMode(dark ? ThemeStore::Dark : ThemeStore::Light);
        for (int step = 0; step < qRound((scale - 1) / .05); ++step)
            session.shell.zoomIn();
        session.router.navigate("settings");
        auto *settings = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(settings);
        QTRY_VERIFY(settings->property("opened").toBool());
        auto *catalog = settings->property("catalog").value<SettingsCatalog *>();
        QVERIFY(catalog);
        catalog->setFilter(QStringLiteral("鉴权头"));
        QCOMPARE(catalog->count(), 1);
        QCOMPARE(catalog->current(), QString("experimental"));
        auto *viewport = visual(session.window->contentItem(), "settingsContentViewport");
        QVERIFY(viewport);
        QQuickItem *view = nullptr;
        QTRY_VERIFY((view = visual(session.window->contentItem(), "settingsViewUserInfo")));
        auto *reset = visual(session.window->contentItem(), "settingsResetDeviceIdentity");
        QVERIFY(reset);
        QCOMPARE(view->height(), 36.);
        QCOMPARE(reset->height(), 36.);
        QVERIFY(reset->property("danger").toBool());
        viewport->setProperty("contentY", qMax(0., viewport->property("contentHeight").toReal() -
                                                       viewport->height()));
        auto *infoOwner = settings->findChild<QObject *>("settingsUserInfoOwner");
        auto *resetOwner = settings->findChild<QObject *>("settingsResetDeviceIdentityOwner");
        QVERIFY(infoOwner && resetOwner);
        QVERIFY(!infoOwner->property("item").value<QObject *>());
        QVERIFY(!resetOwner->property("item").value<QObject *>());
        const auto snapshot = [&](const QString &suffix) {
            const auto dir = qEnvironmentVariable("ECHO_UI_SNAPSHOT_DIR");
            if (dir.isEmpty())
                return true;
            QDir().mkpath(dir);
            return session.window->grabWindow().save(
                dir + "/experimental-identity-" + (dark ? "dark-" : "light-") +
                QString::number(qRound(scale * 100)) + suffix + ".png");
        };
        QTest::qWait(100);
        QVERIFY(snapshot("-rows"));
        view->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QPointer<QObject> info;
        QTRY_VERIFY((info = infoOwner->property("item").value<QObject *>()));
        QTRY_VERIFY(info->property("opened").toBool());
        const auto rawEntries = info->property("entries");
        const auto entries = rawEntries.metaType() == QMetaType::fromType<QJSValue>()
                                 ? rawEntries.value<QJSValue>().toVariant().toList()
                                 : rawEntries.toList();
        bool hasToken = false, hasT1 = false;
        for (const auto &entry : entries) {
            const auto row = entry.toMap();
            hasToken |=
                row.value("label") == "token" && row.value("value") == "synthetic-visible-token";
            hasT1 |= row.value("label") == "t1" && row.value("value") == "synthetic-visible-t1";
        }
        QVERIFY(hasToken && hasT1);
        QVERIFY(snapshot("-viewer"));
        QVERIFY(QMetaObject::invokeMethod(info, "close"));
        QTRY_VERIFY(info.isNull());
        view->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_VERIFY((info = infoOwner->property("item").value<QObject *>()));
        QTRY_VERIFY(info->property("opened").toBool());
        session.api.advanceSession();
        QTRY_VERIFY(info.isNull());
        reset->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QPointer<QObject> confirmation;
        QTRY_VERIFY((confirmation = resetOwner->property("item").value<QObject *>()));
        QTRY_VERIFY(confirmation->property("opened").toBool());
        QVERIFY(snapshot("-reset"));
        auto *cancel = visual(session.window->contentItem(), "settingsCancelDeviceReset");
        QVERIFY(cancel);
        cancel->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_VERIFY(confirmation.isNull());
        QCOMPARE(session.accounts.account.userId(), quint64(42));
        QVERIFY(!experimental.resetPending());
        reset->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_VERIFY((confirmation = resetOwner->property("item").value<QObject *>()));
        catalog->setFilter({});
        QTRY_VERIFY(confirmation.isNull());
        QTRY_VERIFY(visual(session.window->contentItem(), "appearanceMode"));
        QCOMPARE(warnings.size(), 0);
    }
    void productionExperimentalLoggingSettings_data()
    {
        productionNetworkSettingsDraftApplyResetAndTheme_data();
    }
    void productionExperimentalLoggingSettings()
    {
        QFETCH(bool, dark);
        QFETCH(qreal, scale);
        Session session;
        LogService logging(session.directory.filePath("logs"), 2, false, &session.preferences);
        RelaunchService relaunch({}, {}, false);
        QSignalSpy restartNotices(&relaunch, &RelaunchService::notice);
        QVERIFY(logging.active());
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(nullptr, nullptr, &logging, &relaunch));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        session.theme.setFloatingFrosted(false);
        session.theme.setMode(dark ? ThemeStore::Dark : ThemeStore::Light);
        for (int step = 0; step < qRound((scale - 1) / .05); ++step)
            session.shell.zoomIn();
        session.router.navigate("settings");
        auto *dialog = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *catalog = dialog->property("catalog").value<SettingsCatalog *>();
        QVERIFY(catalog);
        catalog->select("experimental");
        QQuickItem *level = nullptr;
        QTRY_VERIFY((level = visual(session.window->contentItem(), "settingsLogLevel")));
        level->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_End);
        QTest::keyClick(session.window, Qt::Key_Return);
        QTRY_COMPARE(logging.level(), QString("error"));
        // 弹层按需创建：回车确认后实例随退出释放，只断言其不再可见。
        QPointer<QObject> popup = level->property("popup").value<QObject *>();
        QVERIFY(popup);
        QTRY_VERIFY(!popup || !popup->property("visible").toBool());
        auto *body = visual(session.window->contentItem(), "settingsLogApiResponse");
        QVERIFY(body);
        body->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_VERIFY(logging.apiResponseBody());
        auto *viewport = visual(session.window->contentItem(), "settingsContentViewport");
        auto *enable = visual(session.window->contentItem(), "settingsEnableDiagnostics");
        QVERIFY(viewport && enable);
        viewport->setProperty("contentY", qMax(0., viewport->property("contentHeight").toReal() -
                                                       viewport->height()));
        QCOMPARE(enable->height(), 36.);
        QCOMPARE(enable->property("text").toString(), QStringLiteral("10 分钟"));
        enable->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_VERIFY(logging.diagnosticActive());
        auto *restart = visual(session.window->contentItem(), "settingsRestartDiagnostics");
        QVERIFY(restart);
        QCOMPARE(restart->property("text").toString(), QStringLiteral("开启启动诊断"));
        QCOMPARE(restart->height(), 36.);
        restart->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_COMPARE(restartNotices.size(), 1);
        auto *disable = visual(session.window->contentItem(), "settingsDisableDiagnostics");
        QVERIFY(disable);
        QTRY_VERIFY(disable->isVisible());
        disable->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_VERIFY(!logging.diagnosticActive());
        catalog->select("interface");
        catalog->select("experimental");
        QTRY_VERIFY((level = visual(session.window->contentItem(), "settingsLogLevel")));
        QCOMPARE(level->property("value").toString(), QString("error"));
        QTRY_COMPARE(level->property("displayText").toString(), QStringLiteral("仅错误"));
        const auto snapshotDirectory = qEnvironmentVariable("ECHO_UI_SNAPSHOT_DIR");
        if (!snapshotDirectory.isEmpty()) {
            QDir().mkpath(snapshotDirectory);
            const auto image = session.window->grabWindow();
            QVERIFY(!image.isNull());
            QVERIFY(image.save(snapshotDirectory + "/logging-" + (dark ? "dark-" : "light-") +
                               QString::number(qRound(scale * 100)) + ".png"));
        }
        viewport = visual(session.window->contentItem(), "settingsContentViewport");
        restart = visual(session.window->contentItem(), "settingsRestartDiagnostics");
        QVERIFY(viewport && restart);
        viewport->setProperty("contentY", qMax(0., viewport->property("contentHeight").toReal() -
                                                       viewport->height()));
        const auto restartBounds = [&] {
            return restart->mapRectToItem(viewport, restart->boundingRect());
        };
        QTRY_VERIFY(restartBounds().top() >= 0 &&
                    restartBounds().bottom() <= viewport->height() + 1);
        if (!snapshotDirectory.isEmpty()) {
            const auto image = session.window->grabWindow();
            QVERIFY(!image.isNull());
            QVERIFY(image.save(snapshotDirectory + "/logging-" + (dark ? "dark-" : "light-") +
                               QString::number(qRound(scale * 100)) + "-bottom.png"));
        }
        QCOMPARE(warnings.size(), 0);
    }
    void productionNetworkSettingsDraftApplyResetAndTheme_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<qreal>("scale");
        QTest::newRow("light-100") << false << qreal(1);
        QTest::newRow("dark-100") << true << qreal(1);
        QTest::newRow("light-125") << false << qreal(1.25);
        QTest::newRow("dark-125") << true << qreal(1.25);
    }
    void productionNetworkSettingsDraftApplyResetAndTheme()
    {
        QFETCH(bool, dark);
        QFETCH(qreal, scale);
        Session session;
        NetworkPreferences network(session.preferences, session.api);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QSignalSpy applied(&network, &NetworkPreferences::applied);
        QVERIFY(session.create(nullptr, &network));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY2(Test::prepareNativeInput(session.window),
                 qPrintable(
                     QString("Settings host: visible=%1 exposed=%2 active=%3 geometry=%4,%5 %6x%7")
                         .arg(session.window->isVisible())
                         .arg(session.window->isExposed())
                         .arg(session.window->isActive())
                         .arg(session.window->x())
                         .arg(session.window->y())
                         .arg(session.window->width())
                         .arg(session.window->height())));
        session.theme.setFloatingFrosted(false);
        session.theme.setMode(dark ? ThemeStore::Dark : ThemeStore::Light);
        for (int step = 0; step < qRound((scale - 1) / .05); ++step)
            session.shell.zoomIn();
        session.router.navigate("settings");
        auto *dialog = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *catalog = dialog->property("catalog").value<SettingsCatalog *>();
        QVERIFY(catalog);
        auto *searchTrigger = visual(session.window->contentItem(), "settingsSearchTrigger");
        QVERIFY(searchTrigger);
        searchTrigger->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Space);
        QQuickItem *searchInput = nullptr;
        QTRY_VERIFY((searchInput = visual(session.window->contentItem(), "settingsSearchInput")) &&
                    searchInput->isVisible());
        QTRY_VERIFY(searchInput->hasActiveFocus());
        for (const auto key : QByteArray("bypass"))
            QTest::keyClick(session.window, key);
        QTRY_COMPARE(catalog->current(), QString("network"));
        QCOMPARE(catalog->rowCount(), 1);
        QTest::keyClick(session.window, Qt::Key_Escape);
        QTRY_COMPARE(catalog->filter(), QString());
        catalog->select("network");
        QQuickItem *mode = nullptr;
        QTRY_VERIFY((mode = visual(session.window->contentItem(), "settingsProxyMode")));
        mode->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_End);
        QTest::keyClick(session.window, Qt::Key_Return);
        QQuickItem *rules = nullptr;
        QTRY_VERIFY((rules = visual(session.window->contentItem(), "settingsProxyRules")) &&
                    rules->isVisible());
        // 弹层按需创建：确认选择后实例随退出释放，只断言其不再可见。
        QPointer<QObject> modePopup = mode->property("popup").value<QObject *>();
        QVERIFY(modePopup);
        QTRY_VERIFY(!modePopup || !modePopup->property("visible").toBool());
        QCOMPARE(network.proxyMode(), QString("direct"));
        QVERIFY(visual(session.window->contentItem(), "settingsProxyUsername"));
        QVERIFY(visual(session.window->contentItem(), "settingsProxyPassword"));
        QPointer<QQuickItem> initialRules(rules);
        // Native keyboard editing changes presentation drafts only.
        rules->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_A, Qt::ControlModifier);
        for (const auto key : QByteArray("http://127.0.0.1:1"))
            QTest::keyClick(session.window, key);
        QCOMPARE(network.proxyRules(), QString());
        auto *viewport = visual(session.window->contentItem(), "settingsContentViewport");
        QVERIFY(viewport);
        auto *popupContent = dialog->property("contentItem").value<QQuickItem *>();
        QVERIFY(popupContent);
        qInfo() << "Network UI renderer API"
                << int(session.window->rendererInterface()->graphicsApi());
#ifdef Q_OS_MACOS
        if (QGuiApplication::platformName() == "cocoa" &&
            qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND") &&
            qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
            QCOMPARE(session.window->rendererInterface()->graphicsApi(),
                     QSGRendererInterface::Metal);
#endif
        const auto popupBounds = popupContent->mapRectToScene(
            QRectF(0, 0, popupContent->width(), popupContent->height()));
        QVERIFY2(popupBounds.left() >= -.5 && popupBounds.top() >= -.5 &&
                     popupBounds.right() <= session.window->width() + .5 &&
                     popupBounds.bottom() <= session.window->height() + .5,
                 qPrintable(QString("Scaled settings outside window: %1,%2 %3x%4")
                                .arg(popupBounds.x())
                                .arg(popupBounds.y())
                                .arg(popupBounds.width())
                                .arg(popupBounds.height())));
        const auto snapshotDirectory = qEnvironmentVariable("ECHO_UI_SNAPSHOT_DIR");
        const auto snapshot = [&](const QString &part) {
            if (snapshotDirectory.isEmpty())
                return;
            QDir().mkpath(snapshotDirectory);
            const auto image = session.window->grabWindow();
            QVERIFY(!image.isNull());
            QVERIFY(image.save(snapshotDirectory + "/network-" + (dark ? "dark-" : "light-") +
                               QString::number(qRound(scale * 100)) + "-" + part + ".png"));
        };
        snapshot("top");
        auto *apply = visual(session.window->contentItem(), "settingsApplyNetwork");
        QVERIFY(apply);
        viewport->setProperty("contentY", qMax(0., viewport->property("contentHeight").toReal() -
                                                       viewport->height()));
        QTRY_VERIFY(apply->mapToItem(viewport, QPointF()).y() >= 0);
        QCOMPARE(apply->height(), 36.);
        const auto surface = dialog->property("contentItem").value<QQuickItem *>();
        QVERIFY(surface);
        const auto controlBounds =
            apply->mapRectToItem(surface, QRectF(0, 0, apply->width(), apply->height()));
        QVERIFY(controlBounds.right() <= surface->width() + 1);
        QVERIFY(Test::prepareNativeInput(session.window));
        apply->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(apply->hasActiveFocus());
        QSignalSpy activated(apply, SIGNAL(clicked()));
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_VERIFY2(
            activated.size() > 0,
            qPrintable(QString("Apply activation failed: focus=%1 enabled=%2 windowActive=%3")
                           .arg(apply->hasActiveFocus())
                           .arg(apply->isEnabled())
                           .arg(session.window->isActive())));
        QTRY_VERIFY2(applied.size() == 1,
                     qPrintable(QString("Apply not committed: error=%1 busy=%2 rules=%3")
                                    .arg(network.errorMessage())
                                    .arg(network.busy())
                                    .arg(rules->property("text").toString())));
        QCOMPARE(network.proxyMode(), QString("fixed_servers"));
        QCOMPARE(network.proxyRules(), QString("http://127.0.0.1:1"));
        snapshot("bottom");
        viewport->setProperty("contentY", 0.);
        rules->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_A, Qt::ControlModifier);
        for (const auto key : QByteArray("unsaved.example:8888"))
            QTest::keyClick(session.window, key);
        catalog->select("interface");
        QTRY_VERIFY(initialRules.isNull());
        catalog->select("network");
        QTRY_VERIFY((rules = visual(session.window->contentItem(), "settingsProxyRules")));
        QCOMPARE(rules->property("text").toString(), QString("http://127.0.0.1:1"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(warnings.size(), 0);
    }
    void productionPlayerEngineSettingsDecimalInputAndLifecycle()
    {
        Session session;
        session.preferences.setValue("playback/volume", 0);
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        session.theme.setFloatingFrosted(false);
        session.router.navigate("settings");
        auto *dialog = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *catalog = dialog->property("catalog").value<SettingsCatalog *>();
        QVERIFY(catalog);
        catalog->select("player");
        QCOMPARE(catalog->current(), QString("player"));
        QQuickItem *buffer = nullptr, *viewport = nullptr;
        QTRY_VERIFY(
            (buffer = visual(session.window->contentItem(), "settingsEngineAudioBufferSecs")));
        QPointer<QQuickItem> retainedBuffer(buffer);
        auto *cache = visual(session.window->contentItem(), "settingsEngineCacheSecs");
        QVERIFY(cache);
        QCOMPARE(visual(cache, "numberInputField")->property("text").toString(),
                 QString("3600000"));
        QVERIFY((viewport = visual(session.window->contentItem(), "settingsContentViewport")));
        // The section is tall: scroll the real viewport before native input.
        viewport->setProperty("contentY", buffer->mapToItem(viewport, QPointF()).y() - 20);
        QTest::qWait(100);
        auto *field = visual(buffer, "numberInputField");
        QVERIFY(field);
        QCOMPARE(field->property("text").toString(), QString("0.2"));
        QTest::mouseClick(
            session.window, Qt::LeftButton, Qt::NoModifier,
            buffer->mapToScene({buffer->width() - 14, buffer->height() / 4}).toPoint());
        QTRY_COMPARE(playback.enginePreferences()->audioBufferSecs(), .25);
        const auto position = field->mapToScene({20, field->height() / 2}).toPoint();
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, position);
        QTest::keyClick(session.window, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(session.window, Qt::Key_0);
        QTest::keyClick(session.window, Qt::Key_Period);
        QTest::keyClick(session.window, Qt::Key_7);
        QTest::keyClick(session.window, Qt::Key_3);
        QTest::keyClick(session.window, Qt::Key_1);
        QTest::keyClick(session.window, Qt::Key_Return);
        QTRY_COMPARE(playback.enginePreferences()->audioBufferSecs(), .731);
        const auto capture = qEnvironmentVariable("ECHO_ENGINE_SETTINGS_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (int width : {1150, 650}) {
                session.window->resize(width, 800);
                viewport->setProperty("contentY", 0);
                QTest::qWait(150);
                QVERIFY(retainedBuffer);
                QCOMPARE(visual(session.window->contentItem(), "settingsEngineAudioBufferSecs"),
                         retainedBuffer.data());
                QVERIFY(viewport->property("contentWidth").toReal() <= viewport->width() + .5);
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/engine-%1-%2.png")
                                      .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                      .arg(width)));
            }
        }
        QPointer<QQuickItem> released(retainedBuffer);
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QTRY_VERIFY(released.isNull());
        QCOMPARE(warnings.size(), 0);
        session.root.reset();
        session.window = nullptr;
        QSignalSpy closed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
    }
    void productionVariableDialogSizingReopens()
    {
        Session session;
        session.shell.setMotionEnabled(false);
        PlaybackService playback(session.api, session.preferences);
        PluginRuntime plugins(session.engine, session.preferences,
                              {playback, session.theme, session.commands, session.router,
                               session.accounts.account, session.api},
                              session.directory.filePath("plugins"), true);
        PluginManager manager(plugins, session.preferences);
        QObject producer;
        TaskCenterModel tasks;
        for (int i = 0; i < 2; ++i) {
            TaskCenterModel::Registration entry;
            entry.id = QString::number(i);
            entry.content.name = i ? "实际变高卡片 中文 Latin" : "后台任务";
            entry.content.percent = i ? 42 : -1;
            tasks.registerTask(producer, entry);
        }
        // Presentation records exercise the production source cards, without network writes.
        manager.sources()->replace({QVariantMap{{"id", "official"},
                                                {"name", "官方源"},
                                                {"url", "https://github.com/example/plugins"},
                                                {"official", true},
                                                {"enabled", true}},
                                    QVariantMap{{"id", "extra"},
                                                {"name", "附加源 Latin"},
                                                {"url", "https://github.com/example/extra"},
                                                {"enabled", true},
                                                {"lastError", "测试错误说明"}}});
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.theme.setFloatingFrosted(false);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        const auto capture = qEnvironmentVariable("ECHO_DIALOG_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const bool source : {true, false}) {
            QQmlComponent component(
                &session.engine,
                QUrl(source ? "qrc:/qt/qml/EchoMusic/Plugins/PluginSourceDialog.qml"
                            : "qrc:/qt/qml/EchoMusic/Shell/TaskCenterDialog.qml"));
            QVariantMap properties{{"parent", QVariant::fromValue(session.window->contentItem())}};
            properties.insert(source ? "manager" : "service",
                              source ? QVariant::fromValue<QObject *>(&manager)
                                     : QVariant::fromValue<QObject *>(&tasks));
            QScopedPointer<QObject> popup(component.createWithInitialProperties(properties));
            QVERIFY2(popup, qPrintable(component.errorString()));
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                session.theme.setMode(mode);
                for (const auto scale : {1., 1.25}) {
                    popup->setProperty("uiScale", scale);
                    qreal stableHeight = 0;
                    for (int cycle = 0; cycle < 4; ++cycle) {
                        QVERIFY(QMetaObject::invokeMethod(popup.data(), "open"));
                        QTRY_VERIFY(popup->property("opened").toBool());
                        auto *list = visual(session.window->contentItem(),
                                            source ? "pluginSourceList" : "taskCenterList");
                        QVERIFY(list);
                        QTRY_COMPARE(list->property("count").toInt(), 2);
                        QTRY_VERIFY(list->property("measuredExtent").toReal() > 0);
                        QTRY_VERIFY(
                            qAbs(list->height() - list->property("measuredExtent").toReal()) < .6);
                        if (!cycle)
                            stableHeight = popup->property("height").toReal();
                        QTRY_COMPARE(popup->property("height").toReal(), stableHeight);
                        if (cycle == 3 && !capture.isEmpty())
                            QVERIFY(session.window->grabWindow().save(
                                capture + QString("/%1-%2-%3.png")
                                              .arg(source ? "sources" : "tasks")
                                              .arg(int(mode))
                                              .arg(scale)));
                        QVERIFY(QMetaObject::invokeMethod(popup.data(), "close"));
                        QTRY_VERIFY(!popup->property("visible").toBool());
                    }
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }

    void settingsDefaultsAndSourceEffectFileManager()
    {
        Session session;
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        session.theme.setFloatingFrosted(false);
        session.router.navigate("settings");
        auto *settings = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(settings);
        QTRY_VERIFY(settings->property("opened").toBool());
        auto *catalog = settings->property("catalog").value<SettingsCatalog *>();
        QVERIFY(catalog);
        QQuickItem *control = nullptr;
        QTRY_VERIFY((control = visual(session.window->contentItem(), "settingsDefaultSearch")));
        QVERIFY(!control->property("checked").toBool());
        catalog->select("playback");
        QTRY_VERIFY((control = visual(session.window->contentItem(), "settingsDefaultQuality")));
        QTRY_COMPARE(control->property("currentText").toString(), QStringLiteral("Hi-Res 品质"));
        QCOMPARE(playback.preferences()->defaultQuality(), QString("hi_res"));
        QQuickItem *compatibility = nullptr;
        QTRY_VERIFY(
            (compatibility = visual(session.window->contentItem(), "settingsCompatibilityMode")));
        QVERIFY(compatibility->property("checked").toBool());
        QVERIFY(playback.preferences()->compatibilityMode());
        QVERIFY(Test::prepareNativeInput(session.window));
        QSignalSpy compatibilityFrame(session.window, &QQuickWindow::frameSwapped);
        session.window->update();
        QTRY_VERIFY(!compatibilityFrame.isEmpty());
        Test::moveNativePointer(
            session.window,
            compatibility->mapToScene({compatibility->width() / 2, compatibility->height() / 2})
                .toPoint());
        QTRY_VERIFY(compatibility->property("hovered").toBool());
        QTest::mouseClick(
            session.window, Qt::LeftButton, Qt::NoModifier,
            compatibility
                ->mapToScene(QPointF(compatibility->width() / 2, compatibility->height() / 2))
                .toPoint());
        QTRY_VERIFY(!playback.preferences()->compatibilityMode());
        QTRY_VERIFY(!compatibility->property("checked").toBool());
        QPointer<QQuickItem> retiredCompatibility = compatibility;
        catalog->select("font");
        QTRY_VERIFY(retiredCompatibility.isNull());
        catalog->select("playback");
        QTRY_VERIFY(
            (compatibility = visual(session.window->contentItem(), "settingsCompatibilityMode")));
        QVERIFY(!compatibility->property("checked").toBool());
        QVERIFY(visualText(session.window->contentItem(),
                           QStringLiteral("首选音质不可用时自动尝试备选")));
        catalog->select("spatialAudio");
        QQuickItem *row = nullptr, *divider = nullptr;
        QTRY_VERIFY((row = visual(session.window->contentItem(), "settingsEffectFilesRow")));
        QVERIFY((divider = visual(session.window->contentItem(), "settingsEffectFilesDivider")));
        QVERIFY(visualText(row, QStringLiteral("音效文件")));
        QVERIFY(!visual(session.window->contentItem(), "settingsLocalEffectFiles"));
        const auto gap = [&] {
            return divider->mapToScene(QPointF()).y() -
                   row->mapToScene(QPointF(0, row->height())).y();
        };
        // The card has two management rows; file lists belong to a dialog.
        QTRY_VERIFY(qAbs(gap() - 20) < .5);
        const auto capture = qEnvironmentVariable("ECHO_SETTINGS_DEFAULTS_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            QTest::qWait(150);
            QVERIFY(qAbs(gap() - 20) < .5);
            if (!capture.isEmpty())
                QVERIFY(session.window->grabWindow().save(capture + (mode == ThemeStore::Dark
                                                                         ? "/effects-dark.png"
                                                                         : "/effects-light.png")));
        }
        auto *manage = visual(session.window->contentItem(), "manageEffectFilesAction");
        QVERIFY(manage);
        QVERIFY(!manage->property("plain").toBool());
        QCOMPARE(manage->height(), 36.);
        QCOMPARE(manage->property("font").value<QFont>().pixelSize(), 12);
        QCOMPARE(manage->property("font").value<QFont>().weight(), QFont::DemiBold);
        QVERIFY(QMetaObject::invokeMethod(manage, "clicked"));
        auto *dialog = session.root->findChild<QObject *>("audioEffectFilesDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QCOMPARE(dialog->property("width").toReal(), 520.);
        auto *files = visual(session.window->contentItem(), "settingsEffectFileList");
        QVERIFY(files);
        QTRY_COMPARE(files->property("count").toInt(), 0);
        QVERIFY(visualText(files, QStringLiteral("暂无本地导入音效")));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            QTest::qWait(150);
            if (!capture.isEmpty())
                QVERIFY(session.window->grabWindow().save(
                    capture + (mode == ThemeStore::Dark ? "/files-empty-dark.png"
                                                        : "/files-empty-light.png")));
        }
        for (int tab = 1; tab < 4; ++tab) {
            auto *button =
                visual(session.window->contentItem(), QString("settingsEffectFileTab-%1").arg(tab));
            QVERIFY(button);
            QCOMPARE(button->height(), 36.);
            QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
            QTRY_COMPARE(dialog->property("currentTab").toInt(), tab);
            QVERIFY(visualText(files, QStringLiteral("此分类暂无已下载音效")));
            QVERIFY(
                visual(session.window->contentItem(), "settingsEffectDownloadHint")->isVisible());
            QVERIFY(
                !visual(session.window->contentItem(), "importLocalEffectsAction")->isVisible());
        }
        dialog->setProperty("currentTab", 0);
        // A readable catalog resource checks file actions, never audio processing.
        const auto path = session.directory.filePath("layout.irs");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("layout-only local resource") > 0);
        file.close();
        playback.audioControls()->registerImpulseResponse(path, QStringLiteral("本地布局检查"));
        QTRY_COMPARE(files->property("count").toInt(), 1);
        QQuickItem *fileRow = nullptr;
        QTRY_VERIFY((fileRow = visual(files, "settingsEffectFileRow")));
        QCOMPARE(fileRow->height(), 52.);
        QTRY_COMPARE(files->height(), 52.);
        const auto singleFileHeight = dialog->property("height").toReal();
        for (int cycle = 0; cycle < 4; ++cycle) {
            QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
            QTRY_VERIFY(!dialog->property("visible").toBool());
            QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
            QTRY_VERIFY(dialog->property("opened").toBool());
            QTRY_COMPARE(files->property("count").toInt(), 1);
            QTRY_COMPARE(files->height(), 52.);
            QTRY_COMPARE(dialog->property("height").toReal(), singleFileHeight);
        }
        QTRY_VERIFY((fileRow = visual(files, "settingsEffectFileRow")));
        auto *rename = visual(fileRow, "settingsEffectRename");
        QVERIFY(rename);
        QVERIFY(QMetaObject::invokeMethod(rename, "clicked"));
        QTRY_COMPARE(dialog->property("editingPath").toString(), path);
        dialog->setProperty("draft", QStringLiteral("新名称.wav"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "saveName"));
        QTRY_COMPARE(
            playback.audioControls()->impulseResponses().at(0).toMap().value("name").toString(),
            QStringLiteral("新名称"));
        QTRY_VERIFY((fileRow = visual(files, "settingsEffectFileRow")));
        rename = visual(fileRow, "settingsEffectRename");
        QVERIFY(rename);
        QVERIFY(QMetaObject::invokeMethod(rename, "clicked"));
        dialog->setProperty("currentTab", 1);
        QVERIFY(dialog->property("editingPath").toString().isEmpty());
        dialog->setProperty("currentTab", 0);
        QTRY_VERIFY((fileRow = visual(files, "settingsEffectFileRow")));
        rename = visual(fileRow, "settingsEffectRename");
        auto *remove = visual(fileRow, "settingsEffectRemove");
        QVERIFY(rename && remove);
        QTRY_COMPARE(rename->width(), 14.);
        QTRY_COMPARE(remove->width(), 14.);
        QTRY_COMPARE(visual(rename, "iconImage")->property("status").toInt(), 1);
        QTRY_COMPARE(visual(remove, "iconImage")->property("status").toInt(), 1);
        QTest::qWait(150);
        if (!capture.isEmpty())
            QVERIFY(session.window->grabWindow().save(capture + "/files-renamed.png"));
        QVERIFY(QMetaObject::invokeMethod(remove, "clicked"));
        QTRY_COMPARE(files->property("count").toInt(), 0);
        // The shared catalog may be large; only visible and cached rows mount.
        for (int index = 0; index < 150; ++index) {
            const auto resource = session.directory.filePath(QString("layout-%1.irs").arg(index));
            QVERIFY(QFile::copy(path, resource));
            playback.audioControls()->registerImpulseResponse(resource, QString::number(index));
        }
        QTRY_COMPARE(files->property("count").toInt(), 150);
        for (const auto scale : {1., 1.25}) {
            session.shell.resetZoom();
            for (int step = 0; step < qRound((scale - 1) * 20); ++step)
                session.shell.zoomIn();
            QTest::qWait(150);
            QCOMPARE(dialog->property("uiScale").toReal(), scale);
            const int mounted = visualCount(files, "settingsEffectFileRow");
            QVERIFY(mounted > 0 && mounted < 30);
            if (!capture.isEmpty())
                QVERIFY(session.window->grabWindow().save(
                    capture + QString("/files-many-%1.png").arg(scale)));
        }
        session.shell.resetZoom();
        QTest::qWait(100);
        QVERIFY(qAbs(gap() - 20) < .5);
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        auto *engines = visual(session.window->contentItem(), "manageDspProvidersAction");
        QVERIFY(engines && !engines->property("plain").toBool());
        QVERIFY(QMetaObject::invokeMethod(engines, "clicked"));
        auto *providerDialog = session.root->findChild<QObject *>("dspProviderDialog");
        QVERIFY(providerDialog);
        QTRY_VERIFY(providerDialog->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(providerDialog, "close"));
        QTRY_VERIFY(!providerDialog->property("visible").toBool());
        QCOMPARE(warnings.size(), 0);
        QVERIFY(QMetaObject::invokeMethod(settings, "close"));
        QTRY_VERIFY(!settings->property("visible").toBool());
        QTest::qWait(150);
        session.root.reset();
        session.window = nullptr;
        QSignalSpy closed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
    }
    void nativeOutputDeviceSelectHasOneDefaultChoice()
    {
        Session session;
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        session.theme.setFloatingFrosted(false);
        session.router.navigate("settings");
        auto *settings = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(settings);
        QTRY_VERIFY(settings->property("opened").toBool());
        auto *catalog = settings->property("catalog").value<SettingsCatalog *>();
        QVERIFY(catalog);
        catalog->select("audioDevice");
        QQuickItem *select = nullptr;
        QTRY_VERIFY((select = visual(session.window->contentItem(), "audioOutputDevice")));
        // This case uses real native enumeration, not a fabricated API sample.
        QTRY_VERIFY_WITH_TIMEOUT(!playback.audioControls()->devices().isEmpty(), 15000);
        const auto capture = qEnvironmentVariable("ECHO_AUDIO_DEVICE_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            auto *popup = select->property("popup").value<QObject *>();
            QVERIFY(popup);
            QVERIFY(QMetaObject::invokeMethod(popup, "open"));
            QTRY_VERIFY(popup->property("opened").toBool());
            const auto names =
                select->property("model").value<QJSValue>().toVariant().toStringList();
            QCOMPARE(names.count(QStringLiteral("系统默认")), 1);
            QVERIFY(!names.contains("Autoselect device"));
            QVERIFY(visualText(session.window->contentItem(), QStringLiteral("系统默认")));
            QTest::qWait(150);
            for (const auto &device : playback.audioControls()->devices()) {
                const auto name = device.toMap().value("name").toString();
                QVERIFY(!name.isEmpty() && name != "auto" && name != "default" && name != "null");
            }
            if (!capture.isEmpty())
                QVERIFY(session.window->grabWindow().save(capture + (mode == ThemeStore::Dark
                                                                         ? "/output-dark.png"
                                                                         : "/output-light.png")));
            QVERIFY(QMetaObject::invokeMethod(popup, "close"));
            QTRY_VERIFY(!popup->property("visible").toBool());
        }
        QCOMPARE(warnings.size(), 0);
        QVERIFY(QMetaObject::invokeMethod(settings, "close"));
        session.root.reset();
        session.window = nullptr;
        QSignalSpy closed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
    }
    void playbackQueueEmptyIndicatorAndCloseIcon()
    {
        const auto cursorPosition = QCursor::pos();
        const auto restoreCursor =
            qScopeGuard([cursorPosition] { QCursor::setPos(cursorPosition); });
        Session session;
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *drawer = session.root->findChild<QObject *>("playbackQueueDrawer");
        QVERIFY(drawer);
        const auto captureDirectory = qEnvironmentVariable("ECHO_THEME_SCREENSHOTS");
        if (!captureDirectory.isEmpty())
            QVERIFY(QDir().mkpath(captureDirectory));
        const auto headerCaptures = qEnvironmentVariable("ECHO_QUEUE_HEADER_CAPTURE_DIR");
        if (!headerCaptures.isEmpty())
            QVERIFY(QDir().mkpath(headerCaptures));
        session.theme.setMotionEnabled(false);
        const auto verifyHeader = [&](const QString &state) {
            auto *body = drawer->property("contentItem").value<QQuickItem *>();
            auto *actions = visual(body, "queueHeaderActions");
            auto *arrows = visual(body, "queueSwitcherArrows");
            auto *close = visual(body, "queueCloseButton");
            QVERIFY(body && actions && arrows && close);
            const auto originalWidth = session.window->width();
            const auto originalMinimum = session.window->minimumWidth();
            session.window->setMinimumWidth(320);
            const auto restore = qScopeGuard([&] {
                session.shell.resetZoom();
                session.window->setWidth(originalWidth);
                session.window->setMinimumWidth(originalMinimum);
            });
            for (qreal scale : {1.0, 1.25}) {
                session.shell.resetZoom();
                while (session.shell.uiScale() < scale - .001)
                    session.shell.zoomIn();
                QTRY_COMPARE(drawer->property("uiScale").toReal(), scale);
                for (int width : {1150, 420}) {
                    session.window->setWidth(width);
                    QTRY_VERIFY(qAbs(actions->mapToItem(body, {actions->width(), 0}).x() -
                                     (body->width() - 16)) < .5);
                    QTRY_COMPARE(actions->width(), 4 * 32.0 + 3 * 4);
                    QQuickItem *previous = nullptr;
                    for (const auto *name : {"queueScrollTopButton", "queueLocateButton",
                                             "queueAddButton", "queueClearButton"}) {
                        auto *button = visual(actions, name);
                        QVERIFY(button && button->isVisible());
                        QCOMPARE(button->width(), 32.0);
                        QCOMPARE(button->height(), 32.0);
                        if (previous) {
                            QCOMPARE(button->x() - previous->x() - previous->width(), 4.0);
                            QCOMPARE(button->y(), previous->y());
                        }
                        previous = button;
                    }
                    QTRY_VERIFY(qAbs(previous->mapToItem(body, {16, 16}).x() -
                                     close->mapToItem(body, {16, 16}).x()) < .5);
                    QCOMPARE(arrows->isVisible(), playback.queueCatalog()->previewCount() > 1);
                    if (arrows->isVisible()) {
                        QCOMPARE(arrows->width(), 2 * 24.0 + 12);
                        QVERIFY(arrows->mapToItem(body, {arrows->width(), 0}).x() <=
                                actions->mapToItem(body, {0, 0}).x() - 10);
                    }
                    if (QGuiApplication::platformName() != "offscreen") {
                        QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                        session.window->update();
                        QTRY_VERIFY(!frames.isEmpty());
                        if (!headerCaptures.isEmpty())
                            QVERIFY(session.window->grabWindow().save(
                                headerCaptures + QString("/queue-%1-%2-%3-%4.png")
                                                     .arg(int(session.theme.mode()))
                                                     .arg(scale)
                                                     .arg(width)
                                                     .arg(state)));
                    }
                }
            }
        };
        for (auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            QTest::mouseMove(session.window, QPoint(20, 400));
            QCOMPARE(playback.queueCatalog()->previewCount(), 0);
            QVERIFY(QMetaObject::invokeMethod(drawer, "open"));
            QTRY_VERIFY(drawer->property("opened").toBool());
            auto *track = visual(session.window->contentItem(), "queueProgressTrack");
            auto *thumb = visual(session.window->contentItem(), "queueProgressThumb");
            auto *close = visual(session.window->contentItem(), "queueCloseButton");
            QVERIFY(track && thumb && close);
            QVERIFY(!track->isVisible());
            QVERIFY(!thumb->isVisible());
            verifyHeader("empty");
            auto *icon = visual(close, "iconImage");
            QVERIFY(icon);
            QTRY_VERIFY(icon->property("source").toUrl().path().startsWith("/x/"));
            QTRY_COMPARE(icon->property("status").toInt(), 1);
            auto expectedInk = session.theme.floatingText();
            expectedInk.setAlphaF(.8);
            QCOMPARE(close->property("foreground").value<QColor>(), expectedInk);
            QCOMPARE(close->width() - close->property("padding").toReal() * 2, 20.);
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(
                    captureDirectory + (mode == ThemeStore::Dark ? "/queue-empty-dark.png"
                                                                 : "/queue-empty-light.png")));

            // The catalogue retains an internal empty default model, but it is
            // not a visible queue. Adding/removing songs updates the projection.
            PlaybackTrack song;
            song.id = "queue-icon-domain";
            song.localPath = "/ui-test/queue-icon.wav";
            song.title = QStringLiteral("队列图标检查");
            playback.appendTracks({song});
            QTRY_COMPARE(playback.queueCatalog()->previewCount(), 1);
            QTRY_VERIFY(!track->isVisible() && !thumb->isVisible());
            auto *list = visual(session.window->contentItem(), "playbackQueueList");
            QVERIFY(list);
            QTRY_COMPARE(list->property("count").toInt(), 1);
            verifyHeader("single");
            QVERIFY(Test::prepareNativeInput(session.window));
            QQuickItem *row = nullptr;
            QTRY_VERIFY((row = visual(session.window->contentItem(), "queueRow")));
            QTest::mouseMove(session.window, row->mapToScene(QPointF(100, 28)).toPoint());
            auto *remove = visual(row, "queueRemoveButton");
            QVERIFY(remove);
            QTRY_VERIFY(remove->isVisible());
            auto *removeIcon = visual(remove, "iconImage");
            QVERIFY(removeIcon);
            QTRY_COMPARE(removeIcon->property("status").toInt(), 1);
            QVERIFY(removeIcon->property("source").toUrl().path().startsWith("/x/"));

            // The indicator counts browsable queues, not songs in a queue.
            auto secondSong = song;
            secondSong.id = "queue-indicator-second-song";
            secondSong.localPath = "/ui-test/queue-indicator-second.wav";
            playback.appendTracks({secondSong});
            QTRY_COMPARE(list->property("count").toInt(), 2);
            QCOMPARE(playback.queueCatalog()->previewCount(), 1);
            QVERIFY(!track->isVisible() && !thumb->isVisible());
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(
                    captureDirectory + (mode == ThemeStore::Dark ? "/queue-single-dark.png"
                                                                 : "/queue-single-light.png")));

            playback.queueCatalog()->activate("queue-indicator/source", "Second queue", {song}, 0);
            QTRY_COMPARE(playback.queueCatalog()->previewCount(), 2);
            QTRY_VERIFY(track->isVisible() && thumb->isVisible());
            verifyHeader("multiple");
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(
                    captureDirectory + (mode == ThemeStore::Dark ? "/queue-multiple-dark.png"
                                                                 : "/queue-multiple-light.png")));
            playback.queueCatalog()->removeViewed();
            QTRY_COMPARE(playback.queueCatalog()->previewCount(), 1);
            QTRY_VERIFY(!track->isVisible() && !thumb->isVisible());
            playback.queueCatalog()->setViewIndex(0);
            playback.queueCatalog()->removeViewed();
            QTRY_COMPARE(playback.queueCatalog()->previewCount(), 0);
            QTRY_VERIFY(!track->isVisible() && !thumb->isVisible());
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                              close->mapToScene(QPointF(16, 16)).toPoint());
            QTRY_VERIFY(!drawer->property("visible").toBool());
            playback.queueCatalog()->clear();
        }
        QCOMPARE(warnings.count(), 0);
    }

    void committedTrackClimaxFromLiveNativeApi()
    {
        if (!qEnvironmentVariableIsSet("ECHO_CLIMAX_LIVE_TEST"))
            QSKIP("Explicit live read-only CDN test");
        Session session;
        session.preferences.setValue("playback/volume", 0);
        QVERIFY(session.api.setNetworkTimeout(10));
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 750);
        QVERIFY(Test::prepareNativeInput(session.window));
        const auto path = session.directory.filePath("climax-diagnostic-silence.wav");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);
        const quint32 bytes = 48000 * 4 * 240;
        out.writeRawData("RIFF", 4);
        out << quint32(36 + bytes);
        out.writeRawData("WAVEfmt ", 8);
        out << quint32(16) << quint16(1) << quint16(2) << quint32(48000) << quint32(192000)
            << quint16(4) << quint16(16);
        out.writeRawData("data", 4);
        out << bytes;
        QVERIFY(file.resize(44 + bytes));
        file.close();
        PlaybackTrack song;
        song.id = "climax-native-diagnostic";
        song.hash = "105A2705BEEE36A8BED029A8E1A6311D";
        song.localPath = path;
        song.title = QStringLiteral("真实高潮元数据 · 诊断静音音轨");
        song.durationMs = 240000;
        playback.playTracks({song}, 0, "Climax verification", "climax-verification");
        QTRY_VERIFY_WITH_TIMEOUT(playback.playing() && !playback.loading(), 15000);
        QTRY_COMPARE_WITH_TIMEOUT(playback.climax()->rowCount(), 1, 15000);
        QCOMPARE(playback.climax()
                     ->data(playback.climax()->index(0), PlaybackClimaxModel::Start)
                     .toDouble(),
                 81000. / 240000);
        QCOMPARE(playback.climax()
                     ->data(playback.climax()->index(0), PlaybackClimaxModel::End)
                     .toDouble(),
                 117100. / 240000);
        auto *page = session.root->findChild<QObject *>("playbackPage");
        QVERIFY(page);
        const auto capture = qEnvironmentVariable("ECHO_CLIMAX_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const bool playingPage : {false, true}) {
                if (playingPage) {
                    QVERIFY(QMetaObject::invokeMethod(page, "open"));
                    QTRY_VERIFY(page->property("opened").toBool());
                }
                auto *bar = playingPage ? page->property("toastAnchor").value<QQuickItem *>()
                                        : visual(session.window->contentItem(), "playerbar");
                QVERIFY(bar);
                // PlayerBar owns both compact and edge-to-edge progress rows;
                // inspect the active row rather than its hidden sibling.
                QQuickItem *slider = nullptr;
                for (auto *candidate : visualAll(bar, "playbackSeekSlider")) {
                    if (candidate->isVisible()) {
                        slider = candidate;
                        break;
                    }
                }
                QVERIFY(slider);
                QQuickItem *tick = nullptr;
                QTRY_VERIFY((tick = visual(slider, "progressClimaxStart-0")));
                auto *end = visual(slider, "progressClimaxEnd-0");
                QVERIFY(end && end->isVisible());
                const auto track = slider->property("background").value<QQuickItem *>();
                QVERIFY(track);
                QVERIFY(qAbs(tick->mapToItem(track, {1, 3}).x() / track->width() -
                             81000. / 240000) < .0001);
                QVERIFY(qAbs(tick->property("color").value<QColor>().alphaF() -
                             (playingPage                 ? .78
                              : theme == ThemeStore::Dark ? .65
                                                          : .8)) < .005);
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture +
                        QString("/%1-%2.png").arg(playingPage ? "page" : "bar").arg(int(theme))));
                if (playingPage) {
                    QVERIFY(QMetaObject::invokeMethod(page, "close"));
                    QTRY_VERIFY(!page->property("visible").toBool());
                }
            }
        }
        // Stop invalidates the model immediately; no old marker survives navigation/replay.
        playback.stop();
        QCOMPARE(playback.climax()->rowCount(), 0);
        QSignalSpy closed(&playback, &PlaybackService::closed),
            apiClosed(&session.api, &ApiClient::closed);
        playback.shutdown();
        session.api.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(apiClosed.size(), 1, 15000);
    }
    void playbackChromeMatchesSource()
    {
        const auto cursor = QCursor::pos();
        const auto restore = qScopeGuard([cursor] { QCursor::setPos(cursor); });
        Session session;
        session.theme.setMotionEnabled(false);
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        // 弹层按需创建：经真实入口所有者触发打开后再获取实例。
        auto *pageOwner = session.root->findChild<QObject *>("playbackPageOwner");
        QVERIFY(pageOwner);
        QVERIFY(
            QMetaObject::invokeMethod(pageOwner, "open", Q_ARG(QVariant, QVariant(QVariantMap{}))));
        QPointer<QObject> page = session.root->findChild<QObject *>("playbackPage");
        QVERIFY(page);
        QTRY_VERIFY(page->property("opened").toBool());
        auto *collapse = visual(session.window->contentItem(), "playbackCollapseAction");
        auto *tools = visual(session.window->contentItem(), "playbackLyricTools");
        auto *source = visual(tools, "lyricSourceAction");
        auto *copy = visual(tools, "lyricTextAction-copy");
        auto *reset = visual(tools, "lyricOffsetAction-0");
        auto *bar = page->property("toastAnchor").value<QQuickItem *>();
        QVERIFY(collapse && tools && source && copy && reset && bar);
        // 添加到歌单要求已登录且有曲目；先准备账号会话和曲目身份。
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        PlaybackServiceTestAccess::quality(playback, "flac");
        QTRY_VERIFY(playback.hasTrack());
        QQuickItem *add = nullptr;
        QTRY_VERIFY((add = visual(bar, "playerbar-action-add-to-playlist")));
        QCOMPARE(add->property("glyph").toString(), QString("text-plus"));
        auto *addImage = visual(add, "iconImage");
        QVERIFY(addImage);
        QTRY_COMPARE(addImage->property("status").toInt(), 1);
        QCOMPARE(addImage->width(), 20.);
        QCOMPARE(add->width(), 28.);
        QCOMPARE(collapse->width(), 34.);
        QCOMPARE(collapse->property("hoverBackground").toBool(), !session.windows.macOS());
        const auto capture = qEnvironmentVariable("ECHO_THEME_SCREENSHOTS");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int i = 0; i < 5; ++i)
                        session.shell.zoomIn();
                const auto center = collapse->mapToScene({17, 17});
                if (!session.windows.macOS()) {
                    QVERIFY(qAbs(center.y() - 23) < .5);
                    QVERIFY(qAbs(collapse->mapToScene({0, 0}).x() - 16) < .5);
                }
                const auto sceneRect = collapse->mapRectToScene(collapse->boundingRect());
                QVERIFY(qAbs(sceneRect.width() - 34) < .5);
                QVERIFY(qAbs(sceneRect.height() - 34) < .5);
                auto *icon = visual(collapse, "iconImage");
                QVERIFY(icon);
                const auto iconRect = icon->mapRectToScene(icon->boundingRect());
                QVERIFY(qAbs(iconRect.width() - 20) < .5);
                QVERIFY(qAbs(iconRect.height() - 20) < .5);
#ifdef Q_OS_MACOS
                if (QGuiApplication::platformName() == "cocoa") {
                    const auto bounds = session.windows.nativeCaptionBounds();
                    QVERIFY(!bounds.isEmpty());
                    QVERIFY(qAbs(collapse->mapToScene({0, 0}).x() - bounds.right() - 8) < .5);
                    QTRY_VERIFY(qAbs(nativeWindowAppearance(session.window)
                                         .value("buttonFrames")
                                         .toList()
                                         .first()
                                         .toRectF()
                                         .center()
                                         .y() -
                                     center.y()) < .5);
                }
#endif
                const auto hover = [&](QQuickItem *item) {
                    Test::moveNativePointer(
                        session.window,
                        item->mapToScene({item->width() / 2, item->height() / 2}).toPoint());
                };
                const auto pixel = [&](QQuickItem *item) {
                    const auto frame = session.window->grabWindow();
                    const auto point =
                        item->mapToScene({4, item->height() / 2}) * frame.devicePixelRatio();
                    return frame.pixelColor(point.toPoint());
                };
                Test::moveNativePointer(session.window, QPoint(400, 200));
                QTRY_VERIFY(!collapse->property("hovered").toBool());
                const auto normal = pixel(collapse);
                hover(collapse);
                QTRY_VERIFY(collapse->property("hovered").toBool());
                if (session.windows.macOS())
                    QCOMPARE(pixel(collapse), normal);
                else
                    QTRY_VERIFY(pixel(collapse) != normal);
                auto *collapseImage = visual(collapse, "iconImage");
                QVERIFY(collapseImage);
                QTRY_COMPARE(collapseImage->property("status").toInt(), 1);
                if (!capture.isEmpty()) {
                    QSignalSpy submitted(session.window, &QQuickWindow::frameSwapped);
                    session.window->update();
                    QTRY_VERIFY(!submitted.isEmpty());
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/collapse-%1-%2.png")
                                      .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                      .arg(zoomed ? 125 : 100)));
#ifdef Q_OS_MACOS
                    if (QGuiApplication::platformName() == "cocoa") {
                        const auto path =
                            capture + QString("/collapse-native-%1-%2.png")
                                          .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                          .arg(zoomed ? 125 : 100);
                        QCOMPARE(QProcess::execute("/usr/sbin/screencapture",
                                                   {"-x", "-o", "-l",
                                                    nativeWindowAppearance(session.window)
                                                        .value("windowNumber")
                                                        .toString(),
                                                    path}),
                                 0);
                    }
#endif
                }
                // Domain-only presentation input; no upstream response or audio fixture.
                playback.lyricText()->clear();
                const auto sourceTop = source->mapToScene(QPointF{}).y();
                QVERIFY(qAbs(tools->y() - qMax(104., 35 / session.shell.uiScale() + 58)) < .5);
                const auto toolsHeight = tools->height();
                playback.lyricText()->replace(
                    {{"offset_ms", 0},
                     {"lines", QJsonArray{QJsonObject{{"text", QStringLiteral("歌词 Song")},
                                                      {"start_ms", 0},
                                                      {"duration_ms", 4000},
                                                      {"words", QJsonArray{}}}}}},
                    "chrome-layout");
                QTRY_COMPARE(tools->height(), toolsHeight);
                QCOMPARE(source->mapToScene(QPointF{}).y(), sourceTop);
                QTRY_COMPARE(tools->property("contentHeight").toReal(), 228.);
                QCOMPARE(reset->opacity(), 0.);
                const auto copyY = copy->mapToItem(tools, QPointF{}).y();
                playback.lyricText()->adjustTrackOffset(1);
                QTRY_COMPARE(reset->opacity(), 1.);
                QCOMPARE(copy->mapToItem(tools, QPointF{}).y(), copyY);
                playback.lyricText()->resetTrackOffset();
                QTRY_COMPARE(reset->opacity(), 0.);
                QCOMPARE(source->width(), 36.);
                QCOMPARE(source->property("glyphSize").toReal(), 14.);
                QCOMPARE(source->property("plain").toBool(), false);
                auto *offset = visual(tools, "lyricOffsetAction--1");
                QVERIFY(offset);
                QCOMPARE(offset->property("glyphSize").toReal(), 15.);
                hover(copy);
                QTRY_VERIFY(copy->property("hovered").toBool());
                auto *background = copy->property("background").value<QQuickItem *>();
                QVERIFY(background && background->isVisible());
                QTRY_VERIFY(background->property("highlighted").toBool());
                auto *copyGlyph = visual(copy, "buttonGlyph");
                QVERIFY(copyGlyph);
                auto *copyImage = visual(copyGlyph, "iconImage");
                QVERIFY(copyImage);
                QTRY_COMPARE(copyImage->property("status").toInt(), 1);
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/tools-%1-%2.png")
                                      .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                      .arg(zoomed ? 125 : 100)));
                playback.lyricText()->replace(
                    {{"offset_ms", 0},
                     {"lines", QJsonArray{QJsonObject{{"text", QStringLiteral("歌词 Song")},
                                                      {"start_ms", 0},
                                                      {"duration_ms", 4000},
                                                      {"words", QJsonArray{}},
                                                      {"translation", "Translation"},
                                                      {"romanization", "yin yi"}}}}},
                    "chrome-layout-translated");
                QTRY_COMPARE(tools->height(), toolsHeight);
                QTRY_COMPARE(tools->property("contentHeight").toReal(), 316.);
                QCOMPARE(source->mapToScene(QPointF{}).y(), sourceTop);
                auto *translation = visual(tools, "lyricTextAction-translation");
                QVERIFY(translation && translation->isVisible());
                playback.lyricText()->setWantTranslation(true);
                QTRY_VERIFY(translation->property("accent").toBool());
                playback.lyricText()->setWantTranslation(false);
                QTRY_VERIFY(!translation->property("accent").toBool());
            }
        }
        QVERIFY(QMetaObject::invokeMethod(page, "close"));
        QTRY_VERIFY(page.isNull()); // 按需弹层关闭后整体释放
        for (const auto &warning : warnings)
            qWarning() << warning;
        QCOMPARE(warnings.size(), 0);
    }
    void playbackToolbarsMatchSourceAcrossWidths()
    {
        const auto cursorPosition = QCursor::pos();
        const auto restoreCursor =
            qScopeGuard([cursorPosition] { QCursor::setPos(cursorPosition); });
        auto clipboard = std::make_unique<QMimeData>();
        const auto *original = QGuiApplication::clipboard()->mimeData();
        if (original) {
            for (const auto &format : original->formats())
                clipboard->setData(format, original->data(format));
        }
        const auto restoreClipboard = qScopeGuard(
            [&clipboard] { QGuiApplication::clipboard()->setMimeData(clipboard.release()); });
        Session session;
        session.preferences.setValue("playback/volume", 0);
        const auto path = session.directory.filePath("toolbar-silence.wav");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        constexpr quint32 bytes = 16000 * 2 * 30;
        stream.writeRawData("RIFF", 4);
        stream << quint32(36 + bytes);
        stream.writeRawData("WAVEfmt ", 8);
        stream << quint32(16) << quint16(1) << quint16(1) << quint32(16000) << quint32(32000)
               << quint16(2) << quint16(16);
        stream.writeRawData("data", 4);
        stream << bytes;
        const QByteArray silence(bytes, '\0');
        QCOMPARE(stream.writeRawData(silence.constData(), silence.size()), silence.size());
        file.close();
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        PlaybackTrack track;
        track.id = "toolbar-local";
        track.localPath = path;
        track.title = QStringLiteral("中文 Song Title — 用于检查长标题和中英文显示的歌曲");
        track.artist = QStringLiteral("Artist 歌手");
        playback.playTracks({track}, 0, "Toolbar", "toolbar-local");
        QTRY_VERIFY_WITH_TIMEOUT(playback.playing() && !playback.loading(), 15000);
        auto *playingToggle = visual(session.window->contentItem(), "playerbar-action-play-toggle");
        QVERIFY(playingToggle);
        QTRY_COMPARE(playingToggle->property("glyph").toString(), QString("pause"));
        auto *playingIcon = visual(visual(playingToggle, "playPausePauseIcon"), "iconImage");
        QVERIFY(playingIcon);
        QTRY_VERIFY(playingIcon->property("source").toUrl().path().startsWith("/pause/"));
        QTRY_COMPARE(playingIcon->property("status").toInt(), 1);
        playback.pause();
        QTRY_VERIFY(!playback.playing());
        QTRY_COMPARE(playingToggle->property("glyph").toString(), QString("play"));
        const auto effectBadge = [&] {
            auto *actions = session.commands.playerbarActions();
            for (int row = 0; row < actions->rowCount(); ++row) {
                const auto index = actions->index(row);
                if (index.data(ShellActionModel::IdRole) == "effect")
                    return index.data(ShellActionModel::BadgeRole).toString();
            }
            return QStringLiteral("missing-action");
        };
        QCOMPARE(effectBadge(), QString());
        playback.audioControls()->applyEqualizerPreset(1);
        QTRY_COMPARE(effectBadge(), QString("EQ"));
        const auto dspLibrary = qEnvironmentVariable("ECHO_DSP_TEST_PROVIDER");
        if (!dspLibrary.isEmpty()) {
            auto *audio = playback.audioControls();
            audio->providers()->importFile(QUrl::fromLocalFile(dspLibrary));
            QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending() && !audio->providerId().isEmpty(),
                                     15000);
            audio->selectProviderPreset("kugou-vinyl");
            QTRY_VERIFY_WITH_TIMEOUT(
                !audio->effectPending() && audio->providerPresetId() == "kugou-vinyl", 15000);
            QTRY_COMPARE(effectBadge(), QString("FX"));
            audio->deactivateProvider();
            QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending() && audio->providerId().isEmpty(),
                                     15000);
            QTRY_COMPARE(effectBadge(), QString("EQ"));
        }
        playback.audioControls()->resetEqualizer();
        QTRY_COMPARE(effectBadge(), QString());
        const auto captureDirectory = qEnvironmentVariable("ECHO_THEME_SCREENSHOTS");
        if (!captureDirectory.isEmpty())
            QVERIFY(QDir().mkpath(captureDirectory));
        auto *bar = visual(session.window->contentItem(), "playerbar");
        auto *center = visual(session.window->contentItem(), "playerbarCenterArea");
        auto *progress = visual(session.window->contentItem(), "playerbarProgressRow");
        auto *rightArea = visual(bar, "playerbarRightArea");
        auto *rightStrip = visual(bar, "playerbarRightActions");
        auto *page = session.root->findChild<QObject *>("playbackPage");
        QVERIFY(bar && center && progress && rightArea && rightStrip && page);
        const auto centered = [](QQuickItem *area, QQuickItem *owner) {
            return qAbs(area->mapToItem(owner, {area->width() / 2, 0}).x() - owner->width() / 2) <
                   0.6;
        };
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const int width : {700, 1150, 1600}) {
                QSignalSpy rendered(session.window, &QQuickWindow::frameSwapped);
                session.window->resize(width, 750);
                QTRY_VERIFY(!rendered.isEmpty());
                QTRY_VERIFY(centered(center, bar));
                QTRY_COMPARE(progress->width(), qMax(0., qMin(center->width(), 480.) - 8));
                // The four default right actions and More occupy 202px.
                // Spare room must not hide Queue, and shown actions must fit.
                if (rightArea->width() >= 202) {
                    QQuickItem *queueButton = nullptr;
                    QTRY_VERIFY((queueButton = visual(rightStrip, "playerbar-action-queue")) &&
                                queueButton->isVisible());
                    QTRY_COMPARE(rightStrip->property("count").toInt(), 4);
                    QVERIFY(rightStrip->width() + 46 <= rightArea->width() + .5);
                }
            }
            session.window->resize(1150, 750);
            QTest::qWait(250);
            auto *title = visual(session.window->contentItem(), "playerbarSongTitle");
            QVERIFY(title);
            QCOMPARE(title->property("font").value<QFont>().weight(), QFont::Bold);
            auto *toggle = visual(session.window->contentItem(), "playerbar-action-play-toggle");
            QVERIFY(toggle);
            QCOMPARE(toggle->width(), 38.);
            // Capture the rendered playing state as well as the paused layout:
            // an accessible Pause action alone does not prove its SVG is visible.
            playback.play();
            QTRY_VERIFY(playback.playing() && !playback.loading());
            QTRY_COMPARE(toggle->property("glyph").toString(), QString("pause"));
            auto *pauseIcon = visual(visual(toggle, "playPausePauseIcon"), "iconImage");
            QVERIFY(pauseIcon);
            QTRY_COMPARE(pauseIcon->property("status").toInt(), 1);
            QTest::qWait(100);
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(captureDirectory +
                                                          (mode == ThemeStore::Dark
                                                               ? "/playerbar-playing-dark.png"
                                                               : "/playerbar-playing-light.png")));
            playback.pause();
            QTRY_VERIFY(!playback.playing());
            QTRY_COMPARE(toggle->property("glyph").toString(), QString("play"));
            auto *seek = visual(progress, "playbackSeekSlider");
            QVERIFY(seek);
            auto *handle = seek->property("handle").value<QQuickItem *>();
            QVERIFY(handle);
            if (QGuiApplication::platformName() != "offscreen")
                QCursor::setPos(session.window->mapToGlobal(QPoint(400, 200)));
            QTest::mouseMove(session.window, QPoint(400, 200));
            QTRY_COMPARE(handle->opacity(), 0.);
            if (QGuiApplication::platformName() != "offscreen") {
                QVERIFY(Test::prepareNativeInput(session.window));
                // The shared desktop can receive real cursor events between
                // synthetic test events. Verify enter/leave in reduced-motion
                // mode, then restore animations before production captures.
                session.theme.setMotionEnabled(false);
                const auto restoreMotion =
                    qScopeGuard([&session] { session.theme.setMotionEnabled(true); });
                const auto enterSeek = [&] {
                    const auto position =
                        seek->mapToScene({seek->width() / 2, seek->height() / 2}).toPoint();
                    // Cocoa may reconcile a synthetic move with the system cursor
                    // on the next frame. Keep both at the same native location.
                    QCursor::setPos(session.window->mapToGlobal(position));
                    QTest::mouseMove(session.window, position);
                    return seek->property("hovered").toBool() && handle->opacity() == 1.;
                };
                QTRY_VERIFY(enterSeek());
                QCursor::setPos(session.window->mapToGlobal(QPoint(400, 200)));
                QTest::mouseMove(session.window, QPoint(400, 200));
                QTRY_COMPARE(handle->opacity(), 0.);
            }
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(
                    captureDirectory +
                    (mode == ThemeStore::Dark ? "/playerbar-dark.png" : "/playerbar-light.png")));
            QVERIFY(QMetaObject::invokeMethod(page, "open"));
            QQuickItem *lyricBar = nullptr;
            QTRY_VERIFY((lyricBar = page->property("toastAnchor").value<QQuickItem *>()));
            auto *shellContent = visual(session.window->contentItem(), "shellContent");
            QVERIFY(shellContent);
            QTRY_VERIFY(page->property("opened").toBool());
            QTRY_VERIFY(!shellContent->isVisible());
            const int profileMs = qEnvironmentVariableIntValue("ECHO_MAIN_PLAYBACK_PROFILE_MS");
            if (profileMs > 0 && mode == ThemeStore::Light) {
                QTRY_VERIFY(page->property("opened").toBool());
                QJsonArray lines;
                for (int i = 0; i < 50; ++i)
                    lines.append(
                        QJsonObject{{"text", QStringLiteral("音乐流动 Native lyric animation")},
                                    {"start_ms", i * 4000},
                                    {"duration_ms", 3500},
                                    {"words", QJsonArray{}},
                                    {"translation", "Translation"}});
                playback.lyricText()->replace({{"offset_ms", 0}, {"lines", lines}},
                                              "profile-render-only");
                playback.play();
                QTRY_VERIFY(playback.playing() && !playback.loading());
                QTest::qWait(500);
                const auto cpuStart = std::clock();
                QElapsedTimer wall;
                wall.start();
                QTest::qWait(profileMs);
                const qreal cpuMs = 1000. * (std::clock() - cpuStart) / CLOCKS_PER_SEC;
                qInfo() << "Main production page: wall ms" << wall.elapsed() << "CPU ms" << cpuMs
                        << "CPU percent" << cpuMs / wall.elapsed() * 100;
                playback.pause();
                QTRY_VERIFY(!playback.playing());
            }
            auto *lyricCenter = visual(lyricBar, "playerbarCenterArea");
            auto *lyricSeek = visual(lyricBar, "playbackSeekSlider");
            QVERIFY(lyricCenter && lyricSeek);
            playback.play();
            QTRY_VERIFY(playback.playing() && !playback.loading());
            QQuickItem *lyricToggle = nullptr;
            QTRY_VERIFY((lyricToggle = visual(lyricBar, "playerbar-action-play-toggle")));
            QTRY_COMPARE(lyricToggle->property("glyph").toString(), QString("pause"));
            auto *lyricIcon = visual(visual(lyricToggle, "playPausePauseIcon"), "iconImage");
            QVERIFY(lyricIcon);
            QTRY_VERIFY(lyricIcon->property("source").toUrl().path().startsWith("/pause/"));
            QTRY_COMPARE(lyricIcon->property("status").toInt(), 1);
            QTest::qWait(250);
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(
                    captureDirectory + (mode == ThemeStore::Dark ? "/lyric-playing-dark.png"
                                                                 : "/lyric-playing-light.png")));
            playback.pause();
            QTRY_VERIFY(!playback.playing());
            QTRY_COMPARE(lyricToggle->property("glyph").toString(), QString("play"));
            QTRY_COMPARE(lyricSeek->width(), lyricBar->width());
            for (const int width : {700, 1150, 1600}) {
                session.window->resize(width, 750);
                QTRY_VERIFY(centered(lyricCenter, lyricBar));
                QTRY_COMPARE(lyricSeek->width(), lyricBar->width());
                auto *cover = visual(session.window->contentItem(), "playbackPageCover");
                auto *coverSide = visual(session.window->contentItem(), "playbackPageCoverSide");
                QVERIFY(cover && coverSide);
                if (width > 768) {
                    QTRY_VERIFY(coverSide->width() <= 420.1);
                    QTRY_VERIFY(cover->width() <= 380.1);
                } else {
                    QTRY_COMPARE(cover->width(), 160.);
                }
            }
            session.window->resize(1150, 750);
            QTest::qWait(250);
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(
                    captureDirectory + (mode == ThemeStore::Dark ? "/lyric-toolbar-dark.png"
                                                                 : "/lyric-toolbar-light.png")));
            session.commands.copyCurrentSongInfo();
            QCOMPARE(QGuiApplication::clipboard()->text(), track.title + " - " + track.artist);
            QVERIFY(QMetaObject::invokeMethod(page, "close"));
            QTRY_VERIFY(!page->property("visible").toBool());
            QTRY_VERIFY(shellContent->isVisible());
            QTRY_VERIFY(bar->property("manageCapacity").toBool());
            QTRY_VERIFY(centered(center, bar));
        }
        for (const auto &warning : warnings)
            qWarning() << warning;
        QCOMPARE(warnings.size(), 0);
        session.root.reset();
        session.window = nullptr;
        QSignalSpy closed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
    }
    void productionPlaybackPopoverSpacingAndEffectMarkers()
    {
        Session session;
        session.preferences.setValue("playback/volume", 0);
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.theme.setFloatingFrosted(false);
        session.window->resize(1150, 1000);
        QVERIFY(Test::prepareNativeInput(session.window));
        QQuickItem anchor(session.window->contentItem());
        anchor.setSize(QSizeF(32, 32));
        anchor.setX(820);
        auto *effect = session.root->findChild<QObject *>("playbackEffectPopup");
        auto *sleep = session.root->findChild<QObject *>("sleepTimerPopup");
        auto *options = session.root->findChild<QObject *>("playbackOptionsPopup");
        auto *more = session.root->findChild<QObject *>("playerbarMorePopup");
        QVERIFY(effect && sleep && options && more);
        const auto capture = qEnvironmentVariable("ECHO_PLAYBACK_POPOVER_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        const QList<QPair<QObject *, QString>> cases = {
            {options, "volume"}, {options, "speed"}, {options, "quality"}, {options, "play-mode"},
            {effect, "effect"},  {sleep, "sleep"},   {more, "more"}};
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1.0, 1.25}) {
                for (const bool above : {true, false}) {
                    anchor.setY(above ? session.window->height() - 50 : 30);
                    for (const auto &[popup, kind] : cases) {
                        popup->setProperty("anchorItem", QVariant::fromValue(&anchor));
                        popup->setProperty("uiScale", scale);
                        if (popup == options)
                            popup->setProperty("kind", kind);
                        if (popup == effect)
                            popup->setProperty("tab", 2);
                        QVERIFY(!popup->property("showArrow").toBool());
                        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
                        QTRY_VERIFY(popup->property("opened").toBool());
                        QTRY_VERIFY(popup->property("opacity").toReal() > .999);
                        auto *body = popup->property("background").value<QQuickItem *>();
                        auto *arrow = body ? visual(body, "anchoredPopupArrow") : nullptr;
                        QVERIFY(arrow && !arrow->isVisible());
                        QCOMPARE(popup->property("sideOffset").toReal(), 8.);
                        QCOMPARE(popup->property("placedAbove").toBool(), above);
                        const auto bodyRect = body->mapRectToScene(body->boundingRect());
                        const auto anchorRect = anchor.mapRectToScene(anchor.boundingRect());
                        const auto gap = above ? anchorRect.top() - bodyRect.bottom()
                                               : bodyRect.top() - anchorRect.bottom();
                        QVERIFY2(qAbs(gap - 8 * scale) < .6,
                                 qPrintable(
                                     QString("%1 gap=%2 scale=%3").arg(kind).arg(gap).arg(scale)));
                        const auto image = session.window->grabWindow();
                        QVERIFY(!image.isNull());
                        if (popup == sleep) {
                            for (const auto minutes : {10, 20, 30, 60}) {
                                auto *button = visual(session.window->contentItem(),
                                                      QString("sleepDuration-%1").arg(minutes));
                                auto *copy =
                                    button ? visual(button, "sleepDurationContent") : nullptr;
                                QVERIFY(button && copy);
                                const auto middle = button->mapToScene(
                                    QPointF(button->width() / 2, button->height() / 2));
                                const auto copyMiddle = copy->mapToScene(
                                    QPointF(copy->width() / 2, copy->height() / 2));
                                QVERIFY(qAbs(copyMiddle.y() - middle.y()) < .5);
                                // Check painted number/unit pixels, not only the container.
                                const auto inkCenter = glyphCenterY(session.window, image, copy);
                                QVERIFY2(qAbs(inkCenter - middle.y()) < 2 * scale,
                                         qPrintable(QString("duration=%1 scale=%2 ink=%3 button=%4")
                                                        .arg(minutes)
                                                        .arg(scale)
                                                        .arg(inkCenter)
                                                        .arg(middle.y())));
                            }
                            for (const auto &selected : {"pause", "quit", "shutdown"}) {
                                auto *button = visual(session.window->contentItem(),
                                                      QString("sleepAction-%1").arg(selected));
                                QVERIFY(button);
                                QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
                                QTRY_COMPARE(sleep->property("selectedAction").toString(),
                                             QString(selected));
                                // Every selection changes the trailing check's visibility.
                                // Both text lines must retain the same scene-space left edge.
                                QTest::qWait(20);
                                qreal left = -1;
                                for (const auto &action : {"pause", "quit", "shutdown"}) {
                                    auto *row = visual(session.window->contentItem(),
                                                       QString("sleepAction-%1").arg(action));
                                    auto *copy = row ? visual(row, "sleepActionContent") : nullptr;
                                    QVERIFY(copy);
                                    const auto x = copy->mapToScene(QPointF()).x();
                                    if (left < 0)
                                        left = x;
                                    QVERIFY(qAbs(x - left) < .5);
                                    for (auto *text : copy->childItems())
                                        if (text->property("text").isValid())
                                            QVERIFY(qAbs(text->mapToScene(QPointF()).x() - left) <
                                                    .5);
                                }
                            }
                            sleep->setProperty("selectedAction", "pause");
                            QTest::qWait(20);
                        }
                        if (!capture.isEmpty())
                            QVERIFY(session.window->grabWindow().save(
                                capture + QString("/%1-%2-%3-%4.png")
                                              .arg(kind)
                                              .arg(theme == ThemeStore::Dark ? "dark" : "light")
                                              .arg(scale)
                                              .arg(above ? "above" : "below")));
                        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                        QTRY_VERIFY(!popup->property("visible").toBool());
                    }
                }
            }
        }
        anchor.setY(session.window->height() - 50);
        effect->setProperty("uiScale", 1.);
        QVERIFY(QMetaObject::invokeMethod(effect, "open"));
        QTRY_VERIFY(effect->property("opened").toBool());
        auto *original = visual(session.window->contentItem(), "originalEffectCurrentDot");
        auto *side =
            visual(visual(session.window->contentItem(), "effectTab-2"), "effectSidebarCurrentDot");
        auto *tabs = visual(session.window->contentItem(), "effectLibraryTabs");
        QVERIFY(original && side && tabs);
        auto *engine = visual(visual(tabs, "tab-0"), "tabCurrentDot");
        auto *mine = visual(visual(tabs, "tab-1"), "tabCurrentDot");
        QVERIFY(engine && mine);
        QVERIFY(original->isVisible());
        QVERIFY(!side->isVisible() && !engine->isVisible() && !mine->isVisible());
        QCOMPARE(visual(original, "statusDotHalo")->size(), QSizeF(8, 8));
        const auto provider = qEnvironmentVariable("ECHO_DSP_TEST_PROVIDER");
        if (!provider.isEmpty()) {
            auto *audio = playback.audioControls();
            audio->providers()->importFile(QUrl::fromLocalFile(provider));
            QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending() && !audio->providerId().isEmpty(),
                                     15000);
            const auto id = audio->providerPresets().first().toMap().value("id").toString();
            audio->selectProviderPreset(id);
            QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending() && audio->providerPresetId() == id,
                                     15000);
            QTRY_VERIFY(side->isVisible() && engine->isVisible());
            QVERIFY(!original->isVisible() && !mine->isVisible());
            for (const auto tab : {2, 0}) {
                effect->setProperty("tab", tab);
                QTest::qWait(230);
                const auto halo = visual(side, "statusDotHalo");
                QVERIFY(halo && halo->isVisible());
                QCOMPARE(halo->size(), QSizeF(11, 11));
                QCOMPARE(side->property("color").value<QColor>(),
                         tab == 2 ? QColor("white") : session.theme.accent());
                const auto image = session.window->grabWindow();
                const auto ratio = image.width() / qreal(session.window->width());
                const auto haloPoint = side->mapToScene(QPointF(6, 2.5));
                const auto outside = side->mapToScene(QPointF(9, 2.5));
                const auto haloPixel = image.pixelColor((haloPoint * ratio).toPoint());
                const auto outsidePixel = image.pixelColor((outside * ratio).toPoint());
                QVERIFY(haloPixel != outsidePixel);
                if (!capture.isEmpty())
                    QVERIFY(image.save(capture + QString("/current-engine-tab-%1.png").arg(tab)));
            }
            audio->selectProviderPreset("");
            QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending(), 15000);
            effect->setProperty("tab", 2);
            QTRY_VERIFY(original->isVisible());
            QVERIFY(!side->isVisible() && !engine->isVisible() && !mine->isVisible());
        }
        // A real, local PCM impulse exercises the other library and source
        // markers without fabricating an upstream response or using user files.
        const auto path = session.directory.filePath("ui-marker-impulse.wav");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream wave(&file);
        wave.setByteOrder(QDataStream::LittleEndian);
        wave.writeRawData("RIFF", 4);
        wave << quint32(36 + 128);
        wave.writeRawData("WAVEfmt ", 8);
        wave << quint32(16) << quint16(1) << quint16(1) << quint32(48000) << quint32(96000)
             << quint16(2) << quint16(16);
        wave.writeRawData("data", 4);
        wave << quint32(128);
        for (int i = 0; i < 64; ++i)
            wave << qint16(i == 0 ? 32767 : 0);
        file.close();
        auto *audio = playback.audioControls();
        audio->registerImpulseResponse(path, QStringLiteral("本机脉冲"));
        audio->selectImpulseResponse(path);
        QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending() && audio->impulseEnabled(), 15000);
        effect->setProperty("libraryTab", 1);
        QTRY_VERIFY(side->isVisible() && mine->isVisible());
        QVERIFY(!engine->isVisible() && !original->isVisible());
        auto *local = visual(session.window->contentItem(), "myEffectSource-0");
        auto *sourceDot = local ? visual(local, "myEffectSourceCurrentDot") : nullptr;
        QVERIFY(sourceDot && sourceDot->isVisible());
        QCOMPARE(visual(sourceDot, "statusDotHalo")->size(), QSizeF(11, 11));
        // Browsing a different category/library must retain the applied marker.
        effect->setProperty("libraryTab", 0);
        QTRY_VERIFY(mine->isVisible() && !engine->isVisible());
        effect->setProperty("libraryTab", 1);
        auto *other = visual(session.window->contentItem(), "myEffectSource-1");
        QVERIFY(other);
        QVERIFY(QMetaObject::invokeMethod(other, "clicked"));
        QTRY_VERIFY(sourceDot->isVisible());
        if (!capture.isEmpty()) {
            QTest::qWait(230);
            QVERIFY(
                session.window->grabWindow().save(capture + "/current-local-other-category.png"));
        }
        audio->setImpulseEnabled(false);
        QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending(), 15000);
        QTRY_VERIFY(original->isVisible());
        QVERIFY(!side->isVisible() && !engine->isVisible() && !mine->isVisible() &&
                !sourceDot->isVisible());
        QCOMPARE(warnings.size(), 0);
    }
    void productionEffectPanelAlignmentAndCompactContentTabs()
    {
        Session session;
        session.preferences.setValue("playback/volume", 0);
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 850);
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *popup = session.root->findChild<QObject *>("playbackEffectPopup");
        QVERIFY(popup);
        const auto provider = qEnvironmentVariable("ECHO_DSP_TEST_PROVIDER");
        if (!provider.isEmpty()) {
            playback.audioControls()->providers()->importFile(QUrl::fromLocalFile(provider));
            QTRY_VERIFY_WITH_TIMEOUT(!playback.audioControls()->effectPending() &&
                                         !playback.audioControls()->providerId().isEmpty(),
                                     15000);
        }
        const auto capture = qEnvironmentVariable("ECHO_EFFECT_ALIGNMENT_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        const auto centerY = [](QQuickItem *item) {
            return item->mapToScene(QPointF(0, item->height() / 2)).y();
        };
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1.0, 1.25}) {
                popup->setProperty("uiScale", scale);
                QVERIFY(QMetaObject::invokeMethod(popup, "open"));
                QTRY_VERIFY(popup->property("opened").toBool());
                for (int tab = 0; tab < 4; ++tab) {
                    popup->setProperty("tab", tab);
                    QTest::qWait(150);
                    const auto image = session.window->grabWindow();
                    QVERIFY(!image.isNull());
                    auto *navigation =
                        visual(session.window->contentItem(), "effectTab-" + QString::number(tab));
                    QVERIFY(navigation);
                    QCOMPARE(navigation->height(), 32.);
                    QCOMPARE(navigation->property("font").value<QFont>().pixelSize(), 13);
                    if (tab != 3) {
                        auto *heading =
                            visual(session.window->contentItem(), "playbackEffectHeading");
                        QVERIFY(heading);
                        QVERIFY(qAbs(glyphCenterY(session.window, image, heading) -
                                     centerY(heading)) < 1.5 * scale);
                    }
                    if (tab == 0) {
                        auto *preset = visual(session.window->contentItem(), "songEffect-none");
                        QVERIFY(preset);
                        auto *label = visual(preset, "effectPresetLabel");
                        QVERIFY(label);
                        QVERIFY(qAbs(glyphCenterY(session.window, image, label) - centerY(preset)) <
                                1.5 * scale);
                    }
                    if (tab == 2) {
                        auto *tabs = visual(session.window->contentItem(), "effectLibraryTabs");
                        QVERIFY(tabs);
                        QCOMPARE(tabs->height(), 32.);
                        QVERIFY(tabs->property("compact").toBool());
                        auto *button = visual(tabs, "tab-0");
                        auto *text = visualText(button, QStringLiteral("引擎预设"));
                        QVERIFY(button && text);
                        QCOMPARE(text->property("font").value<QFont>().pixelSize(), 12);
                        QVERIFY(qAbs(glyphCenterY(session.window, image, text) - centerY(button)) <
                                1.5 * scale);
                        if (!provider.isEmpty()) {
                            auto *name =
                                visual(session.window->contentItem(), "dspProviderNameRow");
                            auto *version = visual(name, "dspProviderVersion");
                            auto *status =
                                visual(session.window->contentItem(), "dspProviderStatusText");
                            QVERIFY(name && version && status);
                            QVERIFY(qAbs(centerY(name) - centerY(status)) < .5);
                            for (auto *row : {name, version, status})
                                QVERIFY(qAbs(glyphCenterY(session.window, image, row) -
                                             centerY(name)) < 1.5 * scale);
                            auto *outputTabs =
                                visual(session.window->contentItem(), "dspOutputDeviceTabs");
                            QVERIFY(outputTabs);
                            QCOMPARE(outputTabs->size(), QSizeF(160, 32));
                            QVERIFY(outputTabs->property("compact").toBool());
                            for (int output = 0; output < 2; ++output) {
                                auto *choice = visual(outputTabs, "tab-" + QString::number(output));
                                QVERIFY(choice);
                                auto *text =
                                    visualText(choice, output == 0 ? QStringLiteral("扬声器")
                                                                   : QStringLiteral("耳机"));
                                QVERIFY(text);
                                QCOMPARE(text->property("font").value<QFont>().pixelSize(), 12);
                                QVERIFY(qAbs(glyphCenterY(session.window, image, text) -
                                             centerY(choice)) < 1.5 * scale);
                            }
                            auto *engine =
                                visual(session.window->contentItem(), "dspProviderEngineCard");
                            auto *heading =
                                visual(session.window->contentItem(), "dspPresetSectionHeading");
                            QVERIFY(engine && heading);
                            const auto bounds = [](QQuickItem *item) {
                                return item->mapRectToScene(QRectF(QPointF(), item->size()));
                            };
                            const auto presets = playback.audioControls()->providerPresets();
                            QVERIFY(presets.size() >= 6);
                            QList<QRectF> options;
                            int headphoneTags = 0, settingsButtons = 0;
                            for (int index = 0; index < 6; ++index) {
                                const auto data = presets[index].toMap();
                                const auto id = data.value("id").toString();
                                auto *option =
                                    visual(session.window->contentItem(), "dspPresetOption-" + id);
                                QVERIFY(option);
                                QCOMPARE(option->height(), 38.);
                                options.append(bounds(option));
                                auto *button = visual(option, "dspPresetButton-" + id);
                                auto *label = visual(option, "dspPresetLabel");
                                auto *tag = visual(option, "dspPresetDeviceTag");
                                auto *settings = visual(option, "dspPresetSettings-" + id);
                                QVERIFY(button && label && tag && settings);
                                QCOMPARE(label->property("font").value<QFont>().pixelSize(), 11);
                                QVERIFY(qAbs(glyphCenterY(session.window, image, label) -
                                             centerY(option)) < 1.5 * scale);
                                const bool headphone =
                                    data.value("recommendedDevice").toString() == "headphone";
                                QCOMPARE(tag->isVisible(), headphone);
                                if (headphone) {
                                    ++headphoneTags;
                                    QCOMPARE(tag->height(), 18.);
                                    QCOMPARE(tag->property("font").value<QFont>().pixelSize(), 10);
                                    QVERIFY(qAbs(bounds(button).right() - bounds(tag).right() -
                                                 9 * scale) < .5);
                                    QVERIFY(qAbs(centerY(tag) - centerY(option)) < .5);
                                    auto *text = visualText(tag, QStringLiteral("耳机"));
                                    QVERIFY(text);
                                    QVERIFY(qAbs(glyphCenterY(session.window, image, text) -
                                                 centerY(option)) < 1.5 * scale);
                                }
                                if (settings->isVisible()) {
                                    ++settingsButtons;
                                    QCOMPARE(settings->size(), QSizeF(28, 28));
                                    QVERIFY(qAbs(bounds(option).right() - bounds(settings).right() -
                                                 4 * scale) < .5);
                                    QVERIFY(qAbs(centerY(settings) - centerY(option)) < .5);
                                }
                            }
                            QVERIFY(headphoneTags > 0 && settingsButtons > 0);
                            for (int row = 0; row < 3; ++row) {
                                const auto &left = options[row * 2];
                                const auto &right = options[row * 2 + 1];
                                QVERIFY(qAbs(left.top() - right.top()) < .5);
                                QVERIFY(qAbs(left.left() - bounds(engine).left()) < .5);
                                QVERIFY(qAbs(right.right() - bounds(engine).right()) < .5);
                                QVERIFY(qAbs(right.left() - left.right() - 7 * scale) < .5);
                                if (row > 0)
                                    QVERIFY(qAbs(left.top() - options[(row - 1) * 2].bottom() -
                                                 7 * scale) < .5);
                            }
                            QVERIFY(qAbs(options[0].top() - bounds(heading).bottom() - 8 * scale) <
                                    .5);
                        }
                    }
                    if (tab == 3) {
                        auto *categories =
                            visual(session.window->contentItem(), "effectPlazaCategoryTabs");
                        auto *sorts = visual(session.window->contentItem(), "effectPlazaSortTabs");
                        QVERIFY(categories && sorts);
                        const QStringList ids{"artist", "headphone", "market"};
                        for (int category = 0; category < ids.size(); ++category) {
                            auto *choice = visual(categories, "tab-" + QString::number(category));
                            QVERIFY(choice);
                            QVERIFY(QMetaObject::invokeMethod(choice, "clicked"));
                            QTRY_COMPARE(playback.effectCatalog()->category(), ids[category]);
                            QTRY_COMPARE(categories->height(), 32.);
                            QCOMPARE(categories->property("currentIndex").toInt(), category);
                            auto *text = visualText(choice, choice->property("text").toString());
                            QVERIFY(text);
                            QCOMPARE(text->property("font").value<QFont>().pixelSize(), 12);
                            if (category == 2) {
                                QTRY_VERIFY(sorts->isVisible());
                                QTRY_COMPARE(sorts->height(), 32.);
                                QTRY_COMPARE(sorts->width(), 220.);
                                auto *latest = visual(sorts, "tab-2");
                                QVERIFY(latest && QMetaObject::invokeMethod(latest, "clicked"));
                                QTRY_COMPARE(sorts->property("currentIndex").toInt(), 2);
                                auto *label = visualText(latest, QStringLiteral("最新"));
                                QVERIFY(label);
                                QCOMPARE(label->property("font").value<QFont>().pixelSize(), 12);
                            } else {
                                QVERIFY(!sorts->isVisible());
                            }
                            QTest::qWait(100);
                            const auto categoryImage = session.window->grabWindow();
                            QVERIFY(qAbs(glyphCenterY(session.window, categoryImage, text) -
                                         centerY(choice)) < 1.5 * scale);
                            if (!capture.isEmpty())
                                QVERIFY(categoryImage.save(
                                    capture +
                                    QString("/plaza-%1-%2-%3.png")
                                        .arg(ids[category])
                                        .arg(theme == ThemeStore::Light ? "light" : "dark")
                                        .arg(scale)));
                        }
                        playback.effectCatalog()->selectCategory("artist");
                    }
                    if (!capture.isEmpty())
                        QVERIFY(image.save(capture +
                                           QString("/effect-%1-%2-%3.png")
                                               .arg(tab)
                                               .arg(theme == ThemeStore::Light ? "light" : "dark")
                                               .arg(scale)));
                }
                QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                QTRY_VERIFY(!popup->property("visible").toBool());
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void productionResolvedQualityMarksPickerAndToolbar()
    {
        Session session;
        session.preferences.setValue("playback/volume", 0);
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        auto *popup = session.root->findChild<QObject *>("playbackOptionsPopup");
        QVERIFY(popup);
        popup->setProperty("kind", "quality");
        auto *actions = session.commands.playerbarActions();
        int qualityRow = -1;
        for (int i = 0; i < actions->rowCount(); ++i)
            if (actions->index(i, 0).data(ShellActionModel::IdRole).toString() == "quality")
                qualityRow = i;
        QVERIFY(qualityRow >= 0);
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            PlaybackServiceTestAccess::quality(playback, "hi_res");
            QVERIFY(QMetaObject::invokeMethod(popup, "open"));
            QTRY_VERIFY(popup->property("opened").toBool());
            auto *hiRes = visual(session.window->contentItem(), "playbackOption-hi_res");
            auto *tape = visual(session.window->contentItem(), "playbackOption-viper_tape");
            QVERIFY(hiRes && tape);
            QTRY_VERIFY(hiRes->property("selected").toBool());
            QVERIFY(!tape->property("selected").toBool());
            QCOMPARE(actions->index(qualityRow, 0).data(ShellActionModel::BadgeRole).toString(),
                     QString("HR"));
            PlaybackServiceTestAccess::quality(playback, "flac");
            auto *flac = visual(session.window->contentItem(), "playbackOption-flac");
            QVERIFY(flac);
            QTRY_VERIFY(flac->property("selected").toBool());
            QVERIFY(!hiRes->property("selected").toBool());
            QCOMPARE(actions->index(qualityRow, 0).data(ShellActionModel::BadgeRole).toString(),
                     QString("SQ"));
            QCOMPARE(playback.quality(), QString("viper_tape"));
            QCOMPARE(playback.preferences()->defaultQuality(), QString("hi_res"));
            QVERIFY(QMetaObject::invokeMethod(popup, "close"));
            QTRY_VERIFY(!popup->property("opened").toBool());
        }
        QSignalSpy closed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
    }
    void productionQualityPopupContainsAndScrollsAllOptions()
    {
        Session session;
        session.preferences.setValue("playback/volume", 0);
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->setMinimumHeight(0);
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *popup = session.root->findChild<QObject *>("playbackOptionsPopup");
        QVERIFY(popup);
        popup->setProperty("kind", "quality");
        QCOMPARE(playback.qualities().size(), 7);
        const auto capture = qEnvironmentVariable("ECHO_QUALITY_POPUP_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        QElapsedTimer inputClock;
        inputClock.start();
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1.0, 1.25}) {
                popup->setProperty("uiScale", scale);
                for (const auto windowHeight : {800, 440}) {
                    session.window->resize(1150, windowHeight);
                    QTest::qWait(100);
                    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
                    QTRY_VERIFY(popup->property("opened").toBool());
                    QTest::qWait(100);
                    auto *view = visual(session.window->contentItem(), "playbackOptionsViewport");
                    auto *last =
                        visual(session.window->contentItem(), "playbackOption-viper_atmos");
                    QVERIFY(view && last);
                    QVERIFY(view->clip());
                    QTRY_VERIFY(view->height() > 0);
                    QTRY_VERIFY(qAbs(view->property("contentY").toReal()) < .5);
                    const auto contentHeight = view->property("contentHeight").toReal();
                    if (windowHeight == 800) {
                        // Seven options fit naturally: no arbitrary 420 DIP cutoff.
                        QTRY_VERIFY(qAbs(view->height() - contentHeight) < .5);
                        // Canonical display inputs exercise both two-line states;
                        // these are UI data, not fabricated upstream API samples.
                        for (const auto &quality : playback.qualities()) {
                            auto data = quality.toMap();
                            const auto key = data.value("value").toString();
                            data["detail"] =
                                key == "standard" ? "2.3 MB" : QStringLiteral("暂无该音质");
                            auto *row =
                                visual(session.window->contentItem(), "playbackOption-" + key);
                            QVERIFY(row);
                            row->setProperty("modelData", data);
                        }
                        QTest::qWait(100);
                        const auto image = session.window->grabWindow();
                        QVERIFY(!image.isNull());
                        const qreal ratio = image.width() / qreal(session.window->width());
                        const auto inkBounds = [&](QQuickItem *text) {
                            const auto bounds =
                                text->mapRectToScene(QRectF(0, 0, text->width(), text->height()));
                            const auto pixels =
                                QRectF(bounds.topLeft() * ratio, bounds.size() * ratio)
                                    .toAlignedRect()
                                    .intersected(image.rect());
                            QHash<QRgb, int> frequencies;
                            for (int y = pixels.top(); y <= pixels.bottom(); ++y)
                                for (int x = pixels.left(); x <= pixels.right(); ++x)
                                    ++frequencies[image.pixel(x, y)];
                            QRgb background = 0;
                            int count = 0;
                            for (auto it = frequencies.cbegin(); it != frequencies.cend(); ++it)
                                if (it.value() > count) {
                                    background = it.key();
                                    count = it.value();
                                }
                            int top = image.height(), bottom = -1;
                            for (int y = pixels.top(); y <= pixels.bottom(); ++y)
                                for (int x = pixels.left(); x <= pixels.right(); ++x) {
                                    const auto pixel = image.pixel(x, y);
                                    if (std::max({qAbs(qRed(pixel) - qRed(background)),
                                                  qAbs(qGreen(pixel) - qGreen(background)),
                                                  qAbs(qBlue(pixel) - qBlue(background))}) > 25) {
                                        top = std::min(top, y);
                                        bottom = std::max(bottom, y);
                                    }
                                }
                            return QPair<int, int>{top, bottom};
                        };
                        qreal tagRight = -1, checkCenter = -1;
                        for (const auto &quality : playback.qualities()) {
                            const auto key = quality.toMap().value("value").toString();
                            auto *row =
                                visual(session.window->contentItem(), "playbackOption-" + key);
                            QVERIFY(row);
                            auto *tag = visual(row, "playbackOptionTag");
                            auto *check = visual(row, "playbackOptionCheck");
                            auto *labels = visual(row, "playbackOptionLabels");
                            auto *title = visual(row, "playbackOptionTitle");
                            auto *detail = visual(row, "playbackOptionDetail");
                            QVERIFY(row && tag && check && labels && title && detail);
                            const auto right = tag->mapToScene(QPointF(tag->width(), 0)).x();
                            const auto center =
                                check->mapToScene(QPointF(check->width() / 2, 0)).x();
                            const auto centerY = [](QQuickItem *item) {
                                return item->mapToScene(QPointF(0, item->height() / 2)).y();
                            };
                            QVERIFY2(
                                qAbs(centerY(labels) - centerY(tag)) < .5,
                                qPrintable(
                                    QString("labels y=%1 h=%2 center=%3 tag y=%4 h=%5 center=%6")
                                        .arg(labels->y())
                                        .arg(labels->height())
                                        .arg(centerY(labels))
                                        .arg(tag->y())
                                        .arg(tag->height())
                                        .arg(centerY(tag))));
                            QVERIFY(qAbs(centerY(check) - centerY(tag)) < .5);
                            QCOMPARE(title->height(), 20.);
                            QCOMPARE(detail->height(), 16.);
                            QCOMPARE(check->size(), QSizeF(14, 14));
                            auto *titleText = title->property("textItem").value<QQuickItem *>();
                            auto *detailText = detail->property("textItem").value<QQuickItem *>();
                            auto *tagText = visualText(tag, tag->property("text").toString());
                            QVERIFY(titleText && detailText && tagText);
                            const auto titleInk = inkBounds(titleText);
                            const auto detailInk = inkBounds(detailText);
                            const auto tagInk = inkBounds(tagText);
                            QVERIFY(titleInk.second >= titleInk.first);
                            QVERIFY(detailInk.second >= detailInk.first);
                            QVERIFY(tagInk.second >= tagInk.first);
                            const auto textCenter =
                                (titleInk.first + detailInk.second + 1) / (2 * ratio);
                            const auto tagCenter = (tagInk.first + tagInk.second + 1) / (2 * ratio);
                            QVERIFY(qAbs(textCenter - centerY(tag)) <= 1.5 * scale);
                            QVERIFY(qAbs(tagCenter - centerY(tag)) <= 1.5 * scale);
                            if (tagRight >= 0) {
                                QVERIFY(qAbs(tagRight - right) < .5);
                                QVERIFY(qAbs(checkCenter - center) < .5);
                            }
                            tagRight = right;
                            checkCenter = center;
                        }
                    } else {
                        QVERIFY(contentHeight > view->height());
                        const auto point =
                            view->mapToScene(QPointF(view->width() / 2, view->height() / 2));
                        QTest::mouseMove(session.window, point.toPoint());
                        QWheelEvent wheel(point, session.window->mapToGlobal(point.toPoint()),
                                          QPoint(), QPoint(0, -2400), Qt::NoButton, Qt::NoModifier,
                                          Qt::NoScrollPhase, false);
                        // Distinct native impulses need increasing timestamps;
                        // repeated timestamp-zero events distort Flickable velocity.
                        wheel.setTimestamp(inputClock.elapsed());
                        QCoreApplication::sendEvent(session.window, &wheel);
                        QTRY_VERIFY(view->property("contentY").toReal() > 0);
                        QTRY_VERIFY(qAbs(view->property("contentY").toReal() + view->height() -
                                         contentHeight) < 1);
                        QTRY_VERIFY(!view->property("moving").toBool());
                    }
                    const auto viewportBounds =
                        view->mapRectToScene(QRectF(0, 0, view->width(), view->height()));
                    const auto lastBounds =
                        last->mapRectToScene(QRectF(0, 0, last->width(), last->height()));
                    QVERIFY(viewportBounds.adjusted(-.5, -.5, .5, .5).contains(lastBounds));
                    QVERIFY(viewportBounds.top() >= 12 * scale - .5);
                    QVERIFY(viewportBounds.bottom() <= session.window->height() - 12 * scale + .5);
                    QTest::qWait(100);
                    if (!capture.isEmpty())
                        QVERIFY(session.window->grabWindow().save(
                            capture + QString("/quality-%1-%2-%3.png")
                                          .arg(theme == ThemeStore::Light ? "light" : "dark")
                                          .arg(scale)
                                          .arg(windowHeight)));
                    QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                    QTRY_VERIFY(!popup->property("visible").toBool());
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void productionEmbeddedLyricsFromActualReader()
    {
        Session session;
        session.theme.setMotionEnabled(false);
        session.theme.setFloatingFrosted(false);
        session.preferences.setValue("playback/volume", 0);
        PlaybackTrack track;
        track.id = "embedded-local";
        track.localPath =
            QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath())
                .absoluteFilePath("../../crates/echo-audio/tests/fixtures/embedded-metadata.flac");
        track.localPath = QDir::cleanPath(track.localPath);
        QVERIFY(QFileInfo::exists(track.localPath));
        const auto key =
            "lyrics/sources/" +
            QString::fromLatin1(
                QCryptographicHash::hash(track.key().toUtf8(), QCryptographicHash::Sha256).toHex());
        session.preferences.setValue(key, "embedded");
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        QVERIFY(Test::prepareNativeInput(session.window));
        playback.playTracks({track}, 0, "Embedded reader", "embedded-reader");
        QTRY_COMPARE_WITH_TIMEOUT(playback.lyricSource()->currentId(), QString("embedded"), 15000);
        QCOMPARE(playback.title(), QString("内嵌标题"));
        QCOMPARE(playback.artist(), QString("内嵌歌手"));
        QCOMPARE(playback.lyrics()->rowCount(), 1);
        QCOMPARE(playback.lyrics()->lineAt(0).value("lineText").toString(), QString("内嵌歌词"));
        QVERIFY(playback.lyrics()->scrollable());
        QVERIFY(playback.lyricSource()->manual());
        QTRY_VERIFY(!playback.cover().isEmpty());
        QVERIFY(QFileInfo(QUrl(playback.cover()).toLocalFile()).exists());
#ifdef Q_OS_MACOS
        QTRY_COMPARE_WITH_TIMEOUT(nativeSystemMediaInfo().value("title").toString(),
                                  QString("内嵌标题"), 10000);
        QCOMPARE(nativeSystemMediaInfo().value("artist").toString(), QString("内嵌歌手"));
        QCOMPARE(nativeSystemMediaInfo().value("album").toString(), QString("内嵌专辑"));
        QTRY_VERIFY_WITH_TIMEOUT(nativeSystemMediaInfo().value("artwork").toBool(), 10000);
#endif
        // Neither reader snapshots nor temporary artwork paths enter retained queues.
        QCOMPARE(playback.queue()->at(0)->title, QString());
        QCOMPARE(playback.queue()->at(0)->cover, QString());
        auto *page = session.root->findChild<QObject *>("playbackPage");
        QVERIFY(page);
        QVERIFY(QMetaObject::invokeMethod(page, "open"));
        QTRY_VERIFY(page->property("opened").toBool());
        auto *tools = visual(session.window->contentItem(), "playbackLyricTools");
        auto *source = visual(tools, "lyricSourceAction");
        QVERIFY(source);
        const auto top = source->mapToScene(QPointF{}).y();
        auto *dialog = page->findChild<QObject *>("lyricSourceDialog");
        QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(playback.lyricSource()->rowCount() > 0);
        auto *embedded = visual(session.window->contentItem(), "lyricSourceChoice-0");
        QVERIFY(embedded);
        QTRY_VERIFY(playback.lyricSource()->selectedIndex() >= 0);
        const auto capture = qEnvironmentVariable("ECHO_EMBEDDED_LYRIC_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            QVERIFY(QDir().mkpath(capture));
            QTest::qWait(100);
            QVERIFY(session.window->grabWindow().save(capture + "/embedded-selection.png"));
        }
        playback.lyricSource()->select(0);
        playback.lyricSource()->applySelected();
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(playback.lyricSource()->currentId(), QString("embedded"));
        playback.lyricText()->clear();
        QCOMPARE(source->mapToScene(QPointF{}).y(), top);
        playback.playIndex(0);
        QTRY_COMPARE_WITH_TIMEOUT(playback.lyricSource()->currentId(), QString("embedded"), 15000);
        playback.stop();
        QCOMPARE(playback.lyricSource()->rowCount(), 0);
        QSignalSpy closed(&playback, &PlaybackService::closed),
            apiClosed(&session.api, &ApiClient::closed);
        playback.shutdown();
        session.api.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(apiClosed.size(), 1, 15000);
    }
    void productionLyricPreviewKeepsSymmetricInsets()
    {
        Session session;
        session.theme.setMotionEnabled(false);
        session.theme.setFloatingFrosted(false);
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 850);
        // The style matrix owns the preview timeline; suppress the idle player's
        // asynchronous position notifications while keeping the real view/model.
        const QSignalBlocker previewClock(&playback);
        QQmlComponent component(&session.engine,
                                QUrl("qrc:/qt/qml/EchoMusic/Playback/LyricSourceDialog.qml"));
        QScopedPointer<QObject> popup(component.createWithInitialProperties(
            {{"parent", QVariant::fromValue(session.window->contentItem())},
             {"playback", QVariant::fromValue(&playback)}}));
        QVERIFY2(popup, qPrintable(component.errorString()));
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        const auto capture = qEnvironmentVariable("ECHO_LYRIC_PREVIEW_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        // Canonical presentation data, without upstream fixtures or live requests.
        QJsonArray rows;
        for (int i = 0; i < 200; ++i)
            rows.append(
                QJsonObject{{"text", i % 2 ? QStringLiteral("中文 Latin 长句歌词换行 ").repeated(6)
                                           : QStringLiteral("周杰伦 - 龙战骑士")},
                            {"start_ms", i * 1000},
                            {"duration_ms", 1000},
                            {"words", QJsonArray{}},
                            {"translation", i % 2 ? "Translation preview" : ""},
                            {"romanization", i % 2 ? "Romanization preview" : ""}});
        PlaybackTrack presentation;
        presentation.localPath = "/presentation/lyric-preview";
        PlaybackServiceTestAccess::lyricMetadata(playback, presentation);
        playback.lyricSource()->updateEmbedded(
            presentation.key(), {}, {}, {{"scrollable", true}, {"offset_ms", 0}, {"lines", rows}});
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const auto scale : {1., 1.25}) {
                popup->setProperty("uiScale", scale);
                for (const int width : {760, 680, 460}) {
                    popup->setProperty("preferredWidth", width);
                    QVERIFY(QMetaObject::invokeMethod(popup.data(), "open"));
                    QTRY_VERIFY(popup->property("opened").toBool());
                    QSignalSpy formFrames(session.window, &QQuickWindow::frameSwapped);
                    session.window->update();
                    QTRY_VERIFY(!formFrames.isEmpty());
                    auto *form = visual(session.window->contentItem(), "lyricSourceSearchForm");
                    auto *title = visual(form, "lyricSourceSearchTitle");
                    auto *artist = visual(form, "lyricSourceSearchArtist");
                    auto *search = visual(form, "lyricSourceSearchAction");
                    auto *reset = visual(session.window->contentItem(), "lyricSourceRefreshAction");
                    QVERIFY(form && title && artist && search && reset);
                    QCOMPARE(title->height(), 36.);
                    QCOMPARE(artist->height(), 36.);
                    QCOMPARE(search->height(), 36.);
                    QCOMPARE(reset->height(), 36.);
                    QVERIFY(qAbs(title->width() / artist->width() - (width <= 480 ? 1. : 1.15)) <
                            .02);
                    // TextInput::boundingRect is its glyph/edit content, not
                    // the field background and control rectangle being aligned.
                    const auto titleBox =
                        title->mapRectToItem(form, QRectF(0, 0, title->width(), title->height()));
                    const auto artistBox = artist->mapRectToItem(
                        form, QRectF(0, 0, artist->width(), artist->height()));
                    const auto searchBox = search->mapRectToItem(form, search->boundingRect());
                    qInfo() << "Lyric search form" << width << scale << form->size() << titleBox
                            << artistBox << searchBox;
                    QVERIFY(qAbs(titleBox.bottom() - artistBox.bottom()) < .5);
                    if (width <= 480)
                        QVERIFY(searchBox.top() >= titleBox.bottom() + 8);
                    else
                        QVERIFY(qAbs(searchBox.bottom() - titleBox.bottom()) < .5);
                    QVERIFY(searchBox.right() <= form->width() + .5);
                    auto *model = playback.lyricSource()->preview();
                    QVERIFY(model->replace({{"scrollable", true}, {"lines", rows}}));
                    auto *view = visual(session.window->contentItem(), "lyricSourcePreview");
                    QVERIFY(view);
                    QTRY_COMPARE(view->property("count").toInt(), 200);
                    QTRY_VERIFY(view->width() > 100 && view->height() > 40);
                    auto *versions = visual(session.window->contentItem(), "lyricSourceCandidates");
                    QVERIFY(versions);
                    if (width > 680)
                        QTRY_VERIFY(qAbs(versions->width() / view->width() - .45 / .55) < .03);

                    for (const int index : {0, 51, 198, 0}) {
                        model->setPosition(index * 1000);
                        QTRY_COMPARE(model->currentIndex(), index);
                        // This is a paint/inset test. Settle the native viewport at
                        // each distant row before probing it, independent of follow timing.
                        QVERIFY(QMetaObject::invokeMethod(view, "forceLayout"));
                        const auto center =
                            view->metaObject()
                                ->enumerator(view->metaObject()->indexOfEnumerator("PositionMode"))
                                .keyToValue("Center");
                        QVERIFY(center >= 0);
                        QVERIFY(QMetaObject::invokeMethod(view, "positionViewAtIndex",
                                                          Q_ARG(int, index), Q_ARG(int, center)));
                        QSignalSpy positioned(session.window, &QQuickWindow::frameSwapped);
                        session.window->update();
                        QTRY_VERIFY(!positioned.isEmpty());
                        QQuickItem *slot = nullptr;
                        const auto findCurrent = [&] {
                            const auto children =
                                view->property("contentItem").value<QQuickItem *>()->childItems();
                            for (auto *child : children)
                                if (child->property("current").isValid() &&
                                    child->property("current").toBool() && child->isVisible() &&
                                    child->y() + child->height() >
                                        view->property("contentY").toReal() &&
                                    child->y() <
                                        view->property("contentY").toReal() + view->height())
                                    return child;
                            return static_cast<QQuickItem *>(nullptr);
                        };
                        QTRY_VERIFY((slot = findCurrent()));
                        // Read the painted surface, not just the outer delegate's box.
                        auto *surface =
                            slot->property("color").isValid()
                                ? slot
                                : visual(slot, "lyricPreviewLine-" + QString::number(index));
                        QVERIFY(surface);
                        QTRY_VERIFY(qAbs(surface->mapToItem(view, {0, 0}).x() - 12.) < .01);
                        QTRY_VERIFY(qAbs(view->width() -
                                         surface->mapToItem(view, {surface->width(), 0}).x() -
                                         12.) < .01);
                        QTRY_VERIFY(surface->height() > 12);
                        QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                        session.window->update();
                        QTRY_VERIFY(!frames.isEmpty());
                        const auto image = session.window->grabWindow();
                        const auto ratio = image.width() / qreal(session.window->width());
                        const auto surfaceTop = surface->mapToItem(view, {0, 0}).y();
                        const auto visibleTop = qMax(0., surfaceTop);
                        const auto visibleBottom =
                            qMin(view->height(), surfaceTop + surface->height());
                        if (visibleBottom - visibleTop <= 1) {
                            qInfo() << "Preview viewport probe" << int(mode) << scale << width
                                    << index << model->currentIndex() << slot->property("index")
                                    << slot->property("current") << view->size()
                                    << view->property("contentY") << slot->position() << surfaceTop
                                    << surface->height();
                            if (!capture.isEmpty())
                                QVERIFY(image.save(capture + "/failed-viewport.png"));
                        }
                        // Vertical clipping is valid in a scroll viewport. Probe its
                        // visible strip away from the surface's rounded corners.
                        QVERIFY(visibleBottom - visibleTop > 1);
                        const auto sampleY = (visibleTop + visibleBottom) / 2;
                        const auto sample = [&](qreal x) {
                            return image.pixelColor(
                                (view->mapToScene({x, sampleY}) * ratio).toPoint());
                        };
                        if (sample(8) == sample(32) && !capture.isEmpty()) {
                            qInfo() << "Preview pixel probe" << int(mode) << scale << width << index
                                    << view->size() << surfaceTop << surface->height() << sampleY;
                            QVERIFY(image.save(capture + "/failed-pixel.png"));
                        }
                        QCOMPARE(sample(8), sample(view->width() - 8));
                        QVERIFY(sample(8) != sample(32));
                        QVERIFY(visualCount(view, "lyricPreviewSlot-" + QString::number(index)) <=
                                1);
                        if (!capture.isEmpty() && index == 0)
                            QVERIFY(image.save(capture + QString("/preview-%1-%2-%3.png")
                                                             .arg(int(mode))
                                                             .arg(scale)
                                                             .arg(width)));
                    }
                    // Recycling after distant timeline jumps stays bounded.
                    int mounted = 0;
                    for (auto *child :
                         view->property("contentItem").value<QQuickItem *>()->childItems())
                        mounted += child->property("current").isValid();
                    QVERIFY(mounted > 0 && mounted < 50);
                    QVERIFY(QMetaObject::invokeMethod(popup.data(), "close"));
                    QTRY_VERIFY(!popup->property("visible").toBool());
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }

    void productionLyricKeywordSearchFromLiveNativeApi()
    {
        if (!qEnvironmentVariableIsSet("ECHO_LYRIC_KEYWORD_LIVE"))
            QSKIP("Opt-in read-only lyric keyword API and native input lifecycle");
        Session session;
        session.theme.setMotionEnabled(false);
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 850);
        QVERIFY(Test::prepareNativeInput(session.window));
        PlaybackTrack track;
        track.id = "keyword-original";
        track.hash = "105A2705BEEE36A8BED029A8E1A6311D";
        track.songId = 32042818;
        track.durationMs = 269000;
        track.title = QStringLiteral("龙战骑士");
        track.artist = QStringLiteral("周杰伦");
        PlaybackServiceTestAccess::lyricMetadata(playback, track);
        auto *sources = playback.lyricSource();
        QTRY_VERIFY_WITH_TIMEOUT(!sources->currentId().isEmpty() && !sources->loading() &&
                                     !sources->previewLoading(),
                                 20000);
        const auto automatic = sources->currentId();
        QQmlComponent component(&session.engine,
                                QUrl("qrc:/qt/qml/EchoMusic/Playback/LyricSourceDialog.qml"));
        QScopedPointer<QObject> popup(component.createWithInitialProperties(
            {{"parent", QVariant::fromValue(session.window->contentItem())},
             {"playback", QVariant::fromValue(&playback)}}));
        QVERIFY2(popup, qPrintable(component.errorString()));
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(QMetaObject::invokeMethod(popup.data(), "open"));
        QTRY_VERIFY(popup->property("opened").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(
            !sources->loading() && !sources->previewLoading() && sources->rowCount() > 1, 20000);
        auto *title = visual(session.window->contentItem(), "lyricSourceSearchTitle");
        auto *artist = visual(session.window->contentItem(), "lyricSourceSearchArtist");
        QVERIFY(title && artist);
        QCOMPARE(title->property("text").toString(), track.title);
        QCOMPARE(artist->property("text").toString(), track.artist);
        for (auto *field : {title, artist}) {
            auto *clear = visual(field, "clearTextAction");
            QVERIFY(clear && clear->isVisible());
            QTest::mouseClick(
                session.window, Qt::LeftButton, {},
                clear->mapToScene({clear->width() / 2, clear->height() / 2}).toPoint());
            QTRY_VERIFY(field->property("text").toString().isEmpty());
        }
        QVERIFY(!sources->canSearch());
        auto *searchAction = visual(session.window->contentItem(), "lyricSourceSearchAction");
        QVERIFY(searchAction && !searchAction->isEnabled());
        const auto type = [&](QQuickItem *field, const QString &value) {
            field->forceActiveFocus(Qt::MouseFocusReason);
            QVERIFY(QMetaObject::invokeMethod(field, "selectAll"));
            QInputMethodEvent input;
            input.setCommitString(value);
            QCoreApplication::sendEvent(field, &input);
            QCOMPARE(field->property("text").toString(), value);
        };
        type(title, QStringLiteral(" 晴天 "));
        type(artist, QStringLiteral(" 周杰伦 "));
        QCOMPARE(sources->searchTitle(), QStringLiteral(" 晴天 "));
        QCOMPARE(sources->searchArtist(), QStringLiteral(" 周杰伦 "));
        QTest::keyClick(session.window, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(
            !sources->loading() && !sources->previewLoading() && sources->rowCount() > 0, 20000);
        QVERIFY2(sources->errorMessage().isEmpty(), qPrintable(sources->errorMessage()));
        QCOMPARE(sources->currentId(), automatic);
        QVERIFY(!sources->index(0).data(Qt::UserRole + 11).toBool());
        QVERIFY(sources->selectedTitle().contains(QStringLiteral("晴天")));
        const auto chosen =
            sources->index(sources->selectedIndex()).data(Qt::UserRole + 1).toString();
        QVERIFY(chosen != automatic);
        const auto artifacts = qEnvironmentVariable("ECHO_LYRIC_SOURCE_CAPTURE_DIR");
        if (!artifacts.isEmpty()) {
            QVERIFY(QDir().mkpath(artifacts));
            QVERIFY(session.window->grabWindow().save(artifacts + "/keyword-results.png"));
        }
        sources->applySelected();
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(sources->currentId(), chosen);
        QVERIFY(sources->manual());
        QVERIFY(QMetaObject::invokeMethod(popup.data(), "open"));
        QTRY_VERIFY_WITH_TIMEOUT(!sources->loading() && !sources->previewLoading() &&
                                     sources->selectedTitle().contains(QStringLiteral("晴天")),
                                 20000);
        QCOMPARE(title->property("text").toString(), track.title);
        sources->restoreAuto();
        QTRY_VERIFY_WITH_TIMEOUT(!popup->property("visible").toBool(), 20000);
        QCOMPARE(sources->currentId(), automatic);
        QVERIFY(!sources->manual());
        QCOMPARE(warnings.size(), 0);
    }
    void productionLyricAutomaticFallbackFromLiveNativeApi()
    {
        if (!qEnvironmentVariableIsSet("ECHO_LYRIC_FALLBACK_LIVE"))
            QSKIP("Opt-in real empty-hash automatic lyric fallback and dialog lifecycle");
        Session session;
        session.theme.setMotionEnabled(false);
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 850);
        QVERIFY(Test::prepareNativeInput(session.window));
        PlaybackTrack track;
        track.id = "automatic-fallback-original";
        track.hash = "DEADBEEFDEADBEEFDEADBEEFDEADBEEF";
        track.durationMs = 269000;
        track.title = QStringLiteral("晴天");
        track.artist = QStringLiteral("周杰伦");
        PlaybackServiceTestAccess::lyricMetadata(playback, track);
        auto *sources = playback.lyricSource();
        QTRY_VERIFY_WITH_TIMEOUT(!sources->loading() && !sources->previewLoading() &&
                                     !sources->currentId().isEmpty(),
                                 20000);
        QVERIFY2(sources->errorMessage().isEmpty(), qPrintable(sources->errorMessage()));
        QVERIFY(sources->selectedTitle().contains(QStringLiteral("晴天")));
        QVERIFY(playback.lyrics()->rowCount() > 20);
        QVERIFY(!sources->manual());
        QVERIFY(session.preferences.allKeys().filter("lyrics/sources/").isEmpty());
        const auto automatic = sources->currentId();
        QQmlComponent component(&session.engine,
                                QUrl("qrc:/qt/qml/EchoMusic/Playback/LyricSourceDialog.qml"));
        QScopedPointer<QObject> popup(component.createWithInitialProperties(
            {{"parent", QVariant::fromValue(session.window->contentItem())},
             {"playback", QVariant::fromValue(&playback)}}));
        QVERIFY2(popup, qPrintable(component.errorString()));
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(QMetaObject::invokeMethod(popup.data(), "open"));
        QTRY_VERIFY(popup->property("opened").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(
            !sources->loading() && !sources->previewLoading() && sources->rowCount() > 1, 20000);
        QCOMPARE(sources->currentId(), automatic);
        QCOMPARE(sources->index(0).data(Qt::UserRole + 1).toString(), automatic);
        QVERIFY(sources->index(0).data(Qt::UserRole + 11).toBool());
        const auto artifacts = qEnvironmentVariable("ECHO_LYRIC_SOURCE_CAPTURE_DIR");
        if (!artifacts.isEmpty()) {
            QVERIFY(QDir().mkpath(artifacts));
            QVERIFY(session.window->grabWindow().save(artifacts + "/automatic-fallback.png"));
        }
        sources->select(1);
        QTRY_VERIFY_WITH_TIMEOUT(!sources->previewLoading() && sources->preview()->rowCount() > 0,
                                 15000);
        const auto manual = sources->index(1).data(Qt::UserRole + 1).toString();
        sources->applySelected();
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(sources->currentId(), manual);
        QVERIFY(sources->manual());
        PlaybackServiceTestAccess::lyricMetadata(playback, track);
        QTRY_VERIFY_WITH_TIMEOUT(!sources->loading() && !sources->previewLoading() &&
                                     sources->currentId() == manual,
                                 20000);
        QVERIFY(QMetaObject::invokeMethod(popup.data(), "open"));
        QTRY_VERIFY_WITH_TIMEOUT(
            !sources->loading() && !sources->previewLoading() && sources->rowCount() > 1, 20000);
        QCOMPARE(sources->index(sources->selectedIndex()).data(Qt::UserRole + 1).toString(),
                 manual);
        sources->restoreAuto();
        QTRY_VERIFY_WITH_TIMEOUT(!popup->property("visible").toBool(), 20000);
        QCOMPARE(sources->currentId(), automatic);
        QVERIFY(!sources->manual());
        QVERIFY(session.preferences.allKeys().filter("lyrics/sources/").isEmpty());
        QCOMPARE(warnings.size(), 0);
    }
    void productionLyricSelectionFromLiveNativeApi()
    {
        if (!qEnvironmentVariableIsSet("ECHO_LYRIC_SOURCE_LIVE_TEST"))
            QSKIP("Explicit read-only live lyric API verification");
        Session session;
        session.theme.setMotionEnabled(false);
        QVERIFY(session.api.setNetworkTimeout(10));
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        PlaybackTrack track;
        track.id = "lyric-read-only-live";
        track.hash = "105A2705BEEE36A8BED029A8E1A6311D";
        track.songId = 32042818;
        track.durationMs = 269000;
        track.title = QStringLiteral("龙战骑士");
        track.artist = QStringLiteral("周杰伦");
        PlaybackServiceTestAccess::lyricMetadata(playback, track);
        auto *sources = playback.lyricSource();
        QTRY_VERIFY_WITH_TIMEOUT(
            !sources->currentId().isEmpty() && playback.lyrics()->rowCount() > 20, 20000);
        const auto automatic = sources->currentId();
        auto *page = session.root->findChild<QObject *>("playbackPage");
        QVERIFY(page);
        QVERIFY(QMetaObject::invokeMethod(page, "open"));
        QTRY_VERIFY(page->property("opened").toBool());
        auto *dialog = session.root->findChild<QObject *>("lyricSourceDialog");
        auto *choose = visual(session.window->contentItem(), "lyricSourceAction");
        QVERIFY(dialog && choose);
        const auto click = [&](QQuickItem *item) {
            const auto position =
                item->mapToScene({item->width() / 2, item->height() / 2}).toPoint();
            Test::moveNativePointer(session.window, position);
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, position);
        };
        Test::moveNativePointer(session.window, choose->mapToScene({18, 18}).toPoint());
        QTRY_VERIFY(choose->isEnabled());
        click(choose);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(
            !sources->loading() && !sources->previewLoading() && sources->rowCount() > 1, 20000);
        QVERIFY2(sources->errorMessage().isEmpty(), qPrintable(sources->errorMessage()));
        QCOMPARE(sources->currentId(), automatic);
        QCOMPARE(sources->index(0).data(Qt::UserRole + 1).toString(), automatic);
        auto *candidates = visual(session.window->contentItem(), "lyricSourceCandidates");
        QVERIFY(candidates);
        QTRY_COMPARE(candidates->property("count").toInt(), sources->rowCount());
        auto *choice = visual(candidates, "lyricSourceChoice-0");
        QVERIFY(choice);
        QCOMPARE(choice->mapToItem(candidates, QPointF{}).x(), 8.);
        int instantiated = 0;
        for (auto *item : candidates->findChildren<QQuickItem *>())
            if (item->objectName().startsWith("lyricSourceCandidate-"))
                ++instantiated;
        QVERIFY(instantiated < sources->rowCount());
        const auto capture = qEnvironmentVariable("ECHO_LYRIC_SOURCE_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const auto scale : {1., 1.25}) {
                dialog->setProperty("uiScale", scale);
                QTest::qWait(100);
                qInfo() << "Lyric dialog geometry" << dialog->property("height") << "scale" << scale
                        << "list" << candidates->size() << "panel"
                        << candidates->parentItem()->size();
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(capture + "/geometry.png"));
                QVERIFY(candidates->height() > 100);
                auto *apply = visual(session.window->contentItem(), "lyricSourceApplyAction");
                QVERIFY(apply && apply->isVisible());
                QVERIFY(apply->mapToScene({0, apply->height()}).y() < session.window->height());
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/sources-%1-%2.png")
                                      .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                      .arg(scale)));
            }
        }
        dialog->setProperty("uiScale", 1.);
        session.window->resize(700, 800);
        QTRY_VERIFY(candidates->width() > 500 && candidates->height() > 100);
        if (!capture.isEmpty())
            QVERIFY(session.window->grabWindow().save(capture + "/sources-narrow.png"));
        session.window->resize(1150, 800);
        QTRY_VERIFY(candidates->width() < 400);
        int staticRow = -1;
        for (int row = 0; row < sources->rowCount(); ++row)
            if (!sources->index(row).data(Qt::UserRole + 9).toBool()) {
                staticRow = row;
                break;
            }
        QVERIFY(staticRow >= 0);
        sources->select(staticRow);
        QTRY_VERIFY_WITH_TIMEOUT(!sources->previewLoading() && sources->preview()->rowCount() > 20,
                                 15000);
        QVERIFY(!sources->preview()->scrollable());
        QCOMPARE(sources->currentId(), automatic);
        QVERIFY(playback.lyrics()->scrollable());
        click(visual(session.window->contentItem(), "lyricSourceApplyAction"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QVERIFY(!playback.lyrics()->scrollable());
        auto *staticView = visual(session.window->contentItem(), "playbackLyricList");
        auto *resume = session.root->findChild<QObject *>("lyricFollowResume");
        QVERIFY(staticView && resume);
        staticView->setProperty("following", false);
        QVERIFY(QMetaObject::invokeMethod(staticView, "movementEnded"));
        QVERIFY(!resume->property("running").toBool());
        QVERIFY(QMetaObject::invokeMethod(resume, "triggered"));
        QVERIFY(!staticView->property("following").toBool());
        QVERIFY(sources->manual());
        const auto selected = sources->currentId();
        Test::moveNativePointer(session.window, choose->mapToScene({18, 18}).toPoint());
        QTRY_VERIFY(choose->isEnabled());
        click(choose);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QCOMPARE(sources->currentId(), selected);
        QTRY_VERIFY(sources->preview()->rowCount() > 20);
        click(visual(session.window->contentItem(), "lyricSourceRestoreAction"));
        QTRY_VERIFY_WITH_TIMEOUT(!dialog->property("visible").toBool(), 20000);
        QCOMPARE(sources->currentId(), automatic);
        QVERIFY(playback.lyrics()->scrollable());
        QVERIFY(!sources->manual());
        QVERIFY(QMetaObject::invokeMethod(page, "close"));
        QTRY_VERIFY(!page->property("visible").toBool());
        for (const auto &warning : warnings)
            qWarning() << warning;
        QCOMPARE(warnings.size(), 0);
    }
    void productionAddToReopenStableHeight()
    {
        Session session;
        PlaybackService playback(session.api, session.preferences);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        PlaybackTrack track;
        track.id = "add-reopen";
        track.localPath = "/ui-test/add-reopen.wav";
        playback.appendTracks({track});
        playback.queueCatalog()->activate("test-second", "Second queue", {track}, 0);
        playback.queueCatalog()->activate("test-third", "Third queue", {track}, 0);
        for (int cycle = 0; cycle < 4; ++cycle) {
            // 弹层按需创建：由服务入口打开后取得新实例，关闭即释放。
            playback.libraryActions()->prepareViewedQueue();
            QPointer<QObject> dialog = session.root->findChild<QObject *>("addToPlaylistDialog");
            QTRY_VERIFY(!dialog.isNull());
            QTRY_VERIFY(dialog->property("opened").toBool());
            auto *targets = visual(session.window->contentItem(), "playlistAddTargets");
            QVERIFY(targets);
            QTRY_COMPARE(targets->property("count").toInt(), 3);
            QTest::qWait(100);
            qInfo() << "Add dialog cycle" << cycle << "height" << dialog->property("height")
                    << "viewport" << targets->height() << "content"
                    << targets->property("contentHeight");
            QVERIFY2(targets->height() >= 155,
                     "Three targets and their section headers must fit on every open");
            QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
            QTRY_VERIFY(dialog.isNull());
        }
    }
    void productionAddToHeaderAlignment()
    {
        Session session;
        session.preferences.setValue("playback/volume", 0);
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        PlaybackTrack track;
        track.id = "header-layout";
        track.localPath = "/ui-test/header.wav";
        track.title = "标题行布局";
        playback.appendTracks({track});
        auto *dialog = session.root->findChild<QObject *>("addToPlaylistDialog");
        QVERIFY(dialog);
        const auto capture = qEnvironmentVariable("ECHO_ADD_TO_HEADER_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1.0, 1.25}) {
                dialog->setProperty("uiScale", scale);
                playback.libraryActions()->prepareViewedQueue();
                QTRY_VERIFY(dialog->property("opened").toBool());
                auto *title = visual(session.window->contentItem(), "dialogHeadingTitle");
                auto *label = visual(session.window->contentItem(), "addToMultipleLabel");
                auto *toggle = visual(session.window->contentItem(), "addToMultipleSwitch");
                auto *close = visual(session.window->contentItem(), "dialogCloseButton");
                QVERIFY(title && label && toggle && close);
                const auto center = [](QQuickItem *item) {
                    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
                };
                QTRY_VERIFY(qAbs(center(title).y() - center(toggle).y()) < .5);
                QTRY_VERIFY(qAbs(center(label).y() - center(toggle).y()) < .5);
                QTRY_VERIFY(qAbs(center(close).y() - center(toggle).y()) < .5);
                QCOMPARE(toggle->size(), QSizeF(36, 20));
                QTest::qWait(100);
                const auto image = session.window->grabWindow();
                QVERIFY(!image.isNull());
                const qreal ratio = image.width() / qreal(session.window->width());
                // Check actual glyph pixels as well as the shared line boxes.
                // A centered Text item can still place fallback Chinese ink too low.
                for (auto *row : {title, label}) {
                    auto *text = row->property("textItem").value<QQuickItem *>();
                    QVERIFY(text);
                    const auto bounds =
                        text->mapRectToScene(QRectF(0, 0, text->width(), text->height()));
                    const auto pixels = QRectF(bounds.topLeft() * ratio, bounds.size() * ratio)
                                            .toAlignedRect()
                                            .intersected(image.rect());
                    int top = image.height(), bottom = -1;
                    for (int y = pixels.top(); y <= pixels.bottom(); ++y)
                        for (int x = pixels.left(); x <= pixels.right(); ++x) {
                            const auto pixel = image.pixelColor(x, y);
                            if (theme == ThemeStore::Dark ? pixel.lightness() > 150
                                                          : pixel.lightness() < 100) {
                                top = std::min(top, y);
                                bottom = std::max(bottom, y);
                            }
                        }
                    QVERIFY(bottom >= top);
                    QVERIFY(qAbs((top + bottom + 1) / (2 * ratio) - center(toggle).y()) <= 1.5);
                }
                QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                                  center(toggle).toPoint());
                QTRY_VERIFY(playback.libraryActions()->multiple());
                QTest::qWait(150);
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/add-to-%1-%2.png")
                                      .arg(theme == ThemeStore::Light ? "light" : "dark")
                                      .arg(scale)));
                QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
                QTRY_VERIFY(!dialog->property("visible").toBool());
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void playerMetadataLinksUseProductionRoutes()
    {
        Session session;
        PlaybackService playback(session.api, session.preferences, nullptr,
                                 session.directory.filePath("playback.sqlite"));
        playback.setVolume(0);
        const auto path = session.directory.filePath("links.wav");
        QVERIFY(writeFloatingTestWave(path));
        PlaybackTrack track;
        track.id = "123";
        track.songId = 123;
        track.localPath = path;
        track.title = "Song";
        track.artist = QStringLiteral("王菲 / Guest");
        track.album = "Album";
        track.albumId = 789;
        track.catalogMetadata.insert(
            "artist_refs", QJsonArray{QJsonObject{{"id", "1234"}, {"name", QStringLiteral("王菲")}},
                                      QJsonObject{{"id", "0"}, {"name", "Guest"}}});
        ArtistDetailService artistPages(session.api, playback, session.router, session.shell,
                                        false);
        AlbumDetailService albumPages(session.api, playback, session.router, false);
        DetailsService details(session.api, playback, session.router, false);
        const auto coverPath = session.directory.filePath("player-cover.png");
        QImage art(120, 120, QImage::Format_RGB32);
        art.fill(QColor("#14a6b8"));
        QVERIFY(art.save(coverPath));
        track.cover = QUrl::fromLocalFile(coverPath).toString();
        QVERIFY(session.create(&playback));
        const auto destroyUi = qScopeGuard([&] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1440, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        playback.playTracks({track}, 0, "Metadata", "metadata-links");
        QTRY_VERIFY_WITH_TIMEOUT(playback.playing(), 15000);
        playback.pause();
        const auto click = [&](QQuickItem *item) {
            QVERIFY(item);
            const auto point = item->mapToScene({item->width() / 2, item->height() / 2}).toPoint();
            Test::moveNativePointer(session.window, point);
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
        };
        auto *actions = session.commands.playerbarActions();
        QVERIFY(actions->setPlacement("host/mv", ShellActionModel::Left));
        for (const bool dark : {false, true})
            for (const qreal scale : {1., 1.25}) {
                session.theme.setMode(dark ? ThemeStore::Dark : ThemeStore::Light);
                session.shell.resetZoom();
                while (session.shell.uiScale() < scale - .001)
                    session.shell.zoomIn();
                auto *artists = visual(session.window->contentItem(), "playerbarArtistLinks");
                QTRY_VERIFY(
                    (artists = visual(session.window->contentItem(), "playerbarArtistLinks")));
                auto *first = visual(artists, "artistLink-0");
                auto *guest = visual(artists, "artistLink-1");
                QVERIFY(first && guest);
                QVERIFY(first->isEnabled());
                QVERIFY(!guest->isEnabled());
                click(first);
                QTRY_COMPARE(session.router.current()->name(), QString("artist-detail"));
                QCOMPARE(session.router.current()->resourceId(), QString("1234"));
                click(guest);
                QCOMPARE(session.router.current()->name(), QString("artist-detail"));
                click(visual(session.window->contentItem(), "playerbarSongTitle"));
                QTRY_COMPARE(session.router.current()->name(), QString("album-detail"));
                QCOMPARE(session.router.current()->resourceId(), QString("789"));
                auto *mv = visual(session.window->contentItem(), "playerbar-action-mv");
                QTRY_VERIFY((mv = visual(session.window->contentItem(), "playerbar-action-mv")));
                QCOMPARE(mv->property("actionIcon").toString(), QString("brand-youtube"));
                QVERIFY(!playback.playing());
                QCOMPARE(playback.queue()->rowCount(), 1);
                const auto captures = qEnvironmentVariable("ECHO_COVER_CAPTURE_DIR");
                if (!captures.isEmpty())
                    QVERIFY(session.window->grabWindow().save(captures +
                                                              QString("/player-links-%1-%2.png")
                                                                  .arg(dark ? "dark" : "light")
                                                                  .arg(scale)));
            }
    }
    void realPlaybackSurfacesAndBoundedFiftyThousandQueue()
    {
        Session session;
        session.preferences.setValue("playback/volume", 0);
        PlaybackService playback(session.api, session.preferences);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&session] {
            session.root.reset();
            session.window = nullptr;
        });
        PlaybackTracks tracks;
        for (int i = 0; i < 50000; ++i) {
            PlaybackTrack track;
            track.id = QString::number(i);
            track.localPath = QString("/ui-test/%1.wav").arg(i);
            track.title = QStringLiteral("中文 Title %1").arg(i);
            track.artist = "Artist 歌手";
            tracks.append(track);
        }
        playback.appendTracks(tracks);
        playback.queue()->setCurrent(34567);
        // Offscreen does not deliver native enter/hover activation. Validate
        // hover on Cocoa/Windows/X11/Wayland, alongside the same formal UI case.
        if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
            QVERIFY(Test::prepareNativeInput(session.window));
            QPointer<QQuickItem> volumeAction;
            const auto hoverVolume = [&] {
                volumeAction = visual(session.window->contentItem(), "playerbar-action-volume");
                if (!volumeAction || volumeAction->width() <= 0 || !volumeAction->isVisible())
                    return false;
                // Cocoa suppresses a native move when the global cursor is
                // already there after a previous test window. Enter from outside.
                QTest::mouseMove(session.window, QPoint(400, 200));
                QTest::mouseMove(
                    session.window,
                    volumeAction
                        ->mapToScene(QPointF(volumeAction->width() / 2, volumeAction->height() / 2))
                        .toPoint());
                return volumeAction && volumeAction->property("hovered").toBool();
            };
            QTRY_VERIFY(hoverVolume());
            auto *actionBackground = volumeAction->property("background").value<QQuickItem *>();
            QVERIFY(actionBackground);
            QCOMPARE(actionBackground->property("color").value<QColor>().alpha(), 0);
            QTest::mouseMove(session.window, QPoint(400, 200));
        }
        auto *drawer = session.root->findChild<QObject *>("playbackQueueDrawer");
        auto *effect = session.root->findChild<QObject *>("playbackEffectPopup");
        auto *sleep = session.root->findChild<QObject *>("sleepTimerPopup");
        auto *options = session.root->findChild<QObject *>("playbackOptionsPopup");
        auto *page = session.root->findChild<QObject *>("playbackPage");
        QVERIFY(drawer && effect && sleep && options && page);
        // Custom artwork chooses white content text while light floating panels
        // must retain their independent foreground, including nested controls.
        const auto captureDirectory = qEnvironmentVariable("ECHO_THEME_SCREENSHOTS");
        if (!captureDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(captureDirectory));
            QVERIFY(Test::prepareNativeInput(session.window));
        }
        session.theme.setMode(ThemeStore::Light);
        QImage artwork(64, 64, QImage::Format_RGB32);
        artwork.fill(QColor("#152c46"));
        const auto artworkFile = session.directory.filePath("playback-artwork.png");
        QVERIFY(artwork.save(artworkFile));
        session.theme.importBackground(QUrl::fromLocalFile(artworkFile));
        QTRY_VERIFY(!session.theme.imageImporting());
        session.theme.setBackgroundText(Qt::white);
        QVERIFY(session.theme.applyBackground());
        QCOMPARE(session.theme.text(), QColor(Qt::white));
        QVERIFY(session.theme.floatingText().lightnessF() < .3);

        QVERIFY(QMetaObject::invokeMethod(drawer, "open"));
        QQuickItem *list = nullptr;
        QTRY_VERIFY((list = visual(session.window->contentItem(), "playbackQueueList")));
        QTRY_VERIFY(!visualAll(session.window->contentItem(), "queueRow").isEmpty());
        const auto rows = visualAll(session.window->contentItem(), "queueRow");
        QVERIFY(rows.size() < 80);
        QCOMPARE(list->property("count").toInt(), 50000);
        QVERIFY(QMetaObject::invokeMethod(drawer, "close"));
        playback.libraryActions()->prepareViewedQueue();
        auto *addDialog = session.root->findChild<QObject *>("addToPlaylistDialog");
        QVERIFY(addDialog);
        QTRY_VERIFY(addDialog->property("opened").toBool());
        QVERIFY(visual(session.window->contentItem(), "playlistAddTargets"));
        QVERIFY(QMetaObject::invokeMethod(addDialog, "close"));
        QVERIFY(QMetaObject::invokeMethod(effect, "open"));
        for (auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (int tab : {0, 1, 2, 3}) {
                effect->setProperty("tab", tab);
                QTest::qWait(100);
                if (tab != 3) {
                    auto *heading = visual(session.window->contentItem(), "playbackEffectHeading");
                    QVERIFY(heading);
                    QCOMPARE(heading->property("color").value<QColor>(),
                             session.theme.floatingText());
                }
                auto *sidebar = visual(session.window->contentItem(), "effectSidebar");
                auto *navigation =
                    visual(session.window->contentItem(), "effectTab-" + QString::number(tab));
                QVERIFY(sidebar && navigation);
                QCOMPARE(sidebar->width(), 88.);
                QCOMPARE(navigation->height(), 32.);
                if (tab == 0) {
                    auto *preset = visual(session.window->contentItem(), "songEffect-none");
                    QVERIFY(preset);
                    QCOMPARE(preset->height(), 38.);
                }
                if (tab == 2) {
                    auto *libraryTabs = visual(session.window->contentItem(), "effectLibraryTabs");
                    QVERIFY(libraryTabs);
                    QCOMPARE(libraryTabs->height(), 32.);
                    effect->setProperty("libraryTab", 1);
                    QTRY_VERIFY(visual(session.window->contentItem(), "myEffectSource-3"));
                }
                // Capture after the shared tab-selection transition settles.
                QTest::qWait(300);
                if (!captureDirectory.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        captureDirectory + QString("/effect-%1-%2.png")
                                               .arg(tab)
                                               .arg(mode == ThemeStore::Dark ? "dark" : "light")));
            }
        }
        effect->setProperty("tab", 1);
        session.theme.setMode(ThemeStore::Light);
        auto *band = visual(session.window->contentItem(), "equalizerBand-0");
        QVERIFY(band && band->height() > 20);
        playback.audioControls()->applyEqualizerPreset(1);
        QCOMPARE(playback.audioControls()->gains().first().toDouble(), 3.);
        const auto dspLibrary = qEnvironmentVariable("ECHO_DSP_TEST_PROVIDER");
        if (!dspLibrary.isEmpty()) {
            auto *audio = playback.audioControls();
            audio->providers()->importFile(QUrl::fromLocalFile(dspLibrary));
            QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending() && !audio->providerId().isEmpty(),
                                     15000);
            effect->setProperty("tab", 1);
            QTRY_VERIFY(!band->isEnabled());
            effect->setProperty("tab", 2);
            effect->setProperty("libraryTab", 0);
            QTest::qWait(300);
            if (!captureDirectory.isEmpty())
                QVERIFY(
                    session.window->grabWindow().save(captureDirectory + "/effect-provider.png"));
            auto *presets = visual(session.window->contentItem(), "dspPresetGrid");
            QVERIFY(presets);
            int presetIndex = 0;
            const auto availablePresets = audio->providerPresets();
            while (presetIndex < availablePresets.size() &&
                   availablePresets[presetIndex].toMap().value("id") != "kugou-vinyl")
                ++presetIndex;
            QVERIFY(presetIndex < availablePresets.size());
            QVERIFY(QMetaObject::invokeMethod(presets, "positionViewAtIndex",
                                              Q_ARG(int, presetIndex), Q_ARG(int, 4)));
            QQuickItem *edit = nullptr;
            QTRY_VERIFY((edit = visual(session.window->contentItem(),
                                       QStringLiteral("dspPresetSettings-kugou-vinyl"))) &&
                        edit->isVisible());
            QCOMPARE(edit->size(), QSizeF(28, 28));
            QVERIFY(qAbs(edit->mapToItem(edit->parentItem(), QPointF(0, edit->height() / 2)).y() -
                         edit->parentItem()->height() / 2) < .5);
            QVERIFY(QMetaObject::invokeMethod(edit, "clicked"));
            QQuickItem *editor = nullptr;
            QTRY_VERIFY((editor = visual(session.window->contentItem(), "dspPresetControlsPanel")));
            auto *sidebar = visual(session.window->contentItem(), "effectSidebar");
            QVERIFY(sidebar);
            QTRY_VERIFY(qAbs(editor->mapToScene({0, 0}).x() -
                             sidebar->mapToScene({sidebar->width(), 0}).x()) < 0.5);
            QTRY_VERIFY(qAbs(editor->mapToScene({0, 0}).y() - sidebar->mapToScene({0, 0}).y()) <
                        0.5);
            const auto controls = audio->providerControls();
            QVERIFY(!controls.isEmpty());
            QQuickItem *control = nullptr;
            QTRY_VERIFY(
                (control = visual(editor, "dspProviderControl-" +
                                              controls.first().toMap().value("id").toString())));
            QTRY_VERIFY(qAbs(control->mapToItem(editor, {0, 0}).x() - 14) < 0.5);
            QTest::qWait(100);
            auto *controlLabel = visual(control, "dspControlLabel");
            auto *controlValue = visual(control, "dspControlValue");
            QVERIFY(controlLabel && controlValue);
            const auto controlCenter =
                controlLabel->mapToScene(QPointF(0, controlLabel->height() / 2)).y();
            QVERIFY(qAbs(controlCenter -
                         controlValue->mapToScene(QPointF(0, controlValue->height() / 2)).y()) <
                    .5);
            const auto controlImage = session.window->grabWindow();
            QVERIFY(qAbs(glyphCenterY(session.window, controlImage, controlLabel) - controlCenter) <
                    1.5);
            QVERIFY(qAbs(glyphCenterY(session.window, controlImage, controlValue) - controlCenter) <
                    1.5);
            QQuickItem *backIcon = nullptr;
            QTRY_VERIFY((backIcon = visual(editor, "iconImage")));
            QTRY_COMPARE(backIcon->property("status").toInt(), 1);
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(captureDirectory +
                                                          "/effect-provider-editor.png"));
            auto *back =
                visual(session.window->contentItem(), QStringLiteral("dspPresetEditorBack"));
            QVERIFY(back);
            QVERIFY(QMetaObject::invokeMethod(back, "clicked"));
            audio->deactivateProvider();
            QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending() && audio->providerId().isEmpty(),
                                     15000);
            QQuickItem *installed = nullptr;
            QTRY_VERIFY(
                (installed = visual(session.window->contentItem(), "dspInstalledProviderCard")) &&
                installed->isVisible());
            QTest::qWait(200);
            if (!captureDirectory.isEmpty())
                QVERIFY(session.window->grabWindow().save(captureDirectory +
                                                          "/effect-installed-provider.png"));
            auto *activate = visual(installed, "activateInstalledDspProvider");
            QVERIFY(activate);
            QVERIFY(QMetaObject::invokeMethod(activate, "clicked"));
            QTRY_VERIFY_WITH_TIMEOUT(!audio->effectPending() && !audio->providerId().isEmpty(),
                                     15000);
        }
        QVERIFY(QMetaObject::invokeMethod(effect, "close"));
        QVERIFY(QMetaObject::invokeMethod(sleep, "open"));
        QTest::qWait(100);
        QVERIFY(sleep->property("height").toDouble() > 180);
        auto *sleepHeading = visualText(session.window->contentItem(), QStringLiteral("定时关闭"));
        QVERIFY(sleepHeading);
        QCOMPARE(sleepHeading->property("color").value<QColor>(), session.theme.floatingText());
        if (!captureDirectory.isEmpty()) {
            session.theme.setFloatingFrosted(true);
            QSignalSpy frame(session.window, &QQuickWindow::frameSwapped);
            session.window->update();
            QTRY_VERIFY(!frame.isEmpty());
            QTest::qWait(100);
            QVERIFY(session.window->grabWindow().save(captureDirectory +
                                                      "/custom-background-sleep-popup.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(sleep, "close"));
        for (const auto &kind : {"volume", "speed", "play-mode", "quality"}) {
            options->setProperty("kind", kind);
            QVERIFY(QMetaObject::invokeMethod(options, "open"));
            QTest::qWait(70);
            if (QString(kind) == "volume") {
                auto *volume = visual(session.window->contentItem(), "playbackVolumeSlider");
                QVERIFY(volume);
                auto *colors = volume->property("colors").value<QObject *>();
                QVERIFY(colors);
                QCOMPARE(colors->property("text").value<QColor>(), session.theme.floatingText());
                if (!captureDirectory.isEmpty()) {
                    const auto originalMode = session.theme.mode();
                    for (auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                        session.theme.setMode(mode);
                        QTest::qWait(100);
                        QVERIFY(session.window->grabWindow().save(
                            captureDirectory + (mode == ThemeStore::Dark
                                                    ? "/volume-popup-dark.png"
                                                    : "/volume-popup-light.png")));
                    }
                    session.theme.setMode(originalMode);
                }
            }
            QVERIFY(QMetaObject::invokeMethod(options, "close"));
        }
        QVERIFY(QMetaObject::invokeMethod(page, "open"));
        QTest::qWait(100);
        QVERIFY(visual(session.window->contentItem(), "playbackLyricList"));
        auto *sourceDialog = page->findChild<QObject *>("lyricSourceDialog");
        QVERIFY(sourceDialog);
        QVERIFY(QMetaObject::invokeMethod(sourceDialog, "open"));
        QTest::qWait(80);
        QVERIFY(QMetaObject::invokeMethod(sourceDialog, "close"));
        QVERIFY(QMetaObject::invokeMethod(page, "close"));
        session.router.navigate("settings");
        auto *settings = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(settings);
        QTRY_VERIFY(settings->property("opened").toBool());
        auto *catalog = settings->property("catalog").value<SettingsCatalog *>();
        QVERIFY(catalog);
        catalog->select("playback");
        QTRY_VERIFY(visual(session.window->contentItem(), "trackTransitionMode"));
        QTRY_VERIFY(visual(session.window->contentItem(), "normalizationReferenceLufs"));
        playback.transition()->setMode("fade");
        QTest::qWait(80);
        QVERIFY(QMetaObject::invokeMethod(settings, "close"));
        for (const auto &warning : warnings)
            qWarning() << warning;
        QCOMPARE(warnings.size(), 0);
        session.root.reset();
        session.window = nullptr;
        QSignalSpy closed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
    }
    void preferencesPreserveUnknownTagsValidateAndKeepFailedDraft()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService accountService(api);
        ListeningPreferencesService service(api, accountService, nullptr);
        QAbstractItemModelTester strengthTester(
            service.strengths(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy resets(service.strengths(), &QAbstractItemModel::modelReset);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        const auto read = ListeningPreferencesTestAccess::read(service, api);
        ListeningPreferencesTestAccess::complete(service, read, preferencesDomain());
        QVERIFY(service.loaded());
        QVERIFY(!service.dirty());
        QCOMPARE(service.modeIndex(), 1);
        QCOMPARE(service.genderOptions().last().toMap().value("value").toString(),
                 QString("other-version"));
        service.setMode(1);
        QVERIFY(!service.dirty());
        service.resetRecommendation();
        QVERIFY(!service.dirty()); // Explicit 50 equals an omitted strength.
        service.setStrength("L1", 20);
        QCOMPARE(resets.size(), 0); // A pressed slider must not lose its delegate.
        auto patch = ListeningPreferencesTestAccess::patch(service);
        QVERIFY(patch.recommendation);
        const auto weights = QJsonDocument::fromJson(patch.recommendation->toUtf8()).object();
        QCOMPARE(weights.value("125").toInt(), 100);
        QCOMPARE(weights.value("L1").toInt(), 20);
        QVERIFY(!patch.languages);
        service.load();
        QVERIFY(!service.loading()); // Never replace an unsaved draft.
        for (const auto &id : QStringList{"L1", "L2", "L5", "L4", "L3", "L6"})
            service.setStrength(id, 0);
        service.save();
        QVERIFY(!service.saving());
        QCOMPARE(service.error(), QStringLiteral("语种不能全部屏蔽，请至少保留一种。"));
        service.setStrength("L1", 50);
        service.setStrength("S9", 100);
        service.setStrength("T1", 0);
        service.save();
        QCOMPARE(service.error(),
                 QStringLiteral("加大 DJ 推荐强度时，请保留翻唱推荐；许多 DJ 歌曲属于翻唱。"));
        service.setStrength("T1", 50);
        const auto write = ListeningPreferencesTestAccess::submitted(service, api);
        ListeningPreferencesTestAccess::fail(service, write);
        QVERIFY(!service.saving());
        QVERIFY(service.dirty());
        QVERIFY(!service.error().isEmpty());
        const auto retried = ListeningPreferencesTestAccess::submitted(service, api);
        QSignalSpy saved(&service, &ListeningPreferencesService::saved);
        ListeningPreferencesTestAccess::complete(service, retried, R"({"updated":true})");
        QCOMPARE(saved.size(), 1);
        QVERIFY(!service.dirty());
        const auto stale = ListeningPreferencesTestAccess::read(service, api);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900002)));
        ListeningPreferencesTestAccess::complete(service, stale, "{}");
        QVERIFY(!service.loaded());
        service.shutdown();
        api.shutdown();
    }
    void missingMembershipCodesKeepProductsWithoutInventingKnownRights()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService accountService(api);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        const auto read = AccountServiceTestAccess::vipRequest(accountService, api);
        AccountServiceTestAccess::complete(
            accountService, read,
            R"({"user_id":900001,"user_type_code":null,"vip_type_code":null,"products":[{"product":"svip","business":"concept","active":null,"active_code":99}]})");
        QVERIFY(accountService.profileError().isEmpty());
        const auto membership = AccountServiceTestAccess::membershipDomain(accountService);
        QCOMPARE(membership["products"].toArray().size(), 1);
        QVERIFY(!AccountServiceTestAccess::membershipKnown(accountService));
        QVERIFY(accountService.memberships().isEmpty());
        QCOMPARE(accountService.membershipLabel(), QString("未确认"));
        api.shutdown();
    }
    void missingPreferenceFactsDoNotBecomeExplicitClears()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService accountService(api);
        ListeningPreferencesService service(api, accountService, nullptr);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        const auto read = ListeningPreferencesTestAccess::read(service, api);
        ListeningPreferencesTestAccess::complete(
            service, read,
            R"({"recommendation_mode":null,"gender":null,"age":null,"languages":{"encoded":null,"weights":null},"styles":{"encoded":null,"weights":null},"recommendation":{"encoded":null,"weights":null}})");
        QVERIFY(service.loaded());
        QVERIFY(!service.dirty());
        QCOMPARE(service.modeIndex(), -1);
        QCOMPARE(service.genderOptions().first().toMap()["label"].toString(), QString("未提供"));
        service.setGender(1); // Explicit "不设置" differs from missing.
        auto patch = ListeningPreferencesTestAccess::patch(service);
        QVERIFY(patch.gender.has_value());
        QVERIFY(patch.gender->isEmpty());
        QVERIFY(!patch.mode && !patch.age && !patch.languages && !patch.styles &&
                !patch.recommendation);
        service.discard();
        QVERIFY(!service.dirty());
        service.setMode(1);
        patch = ListeningPreferencesTestAccess::patch(service);
        QCOMPARE(patch.mode, std::optional<QString>("0"));
        service.shutdown();
        api.shutdown();
    }
    void profileWritesRejectInvalidDraftAndCancelOlderRead()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService accountService(api);
        ProfileService profile(api, accountService);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        profile.saveProfile("  ", 2, "", "", "", "");
        QVERIFY(!profile.saving());
        QCOMPARE(profile.editorError(), QStringLiteral("昵称不能为空"));
        profile.saveProfile(accountService.nickname(), 2, "2099-01-01", "", "", "");
        QCOMPARE(profile.editorError(), QStringLiteral("请选择有效生日"));
        profile.saveProfile(accountService.nickname(), 2, "", QString(121, 'a'), "", "");
        QCOMPARE(profile.editorError(), QStringLiteral("个性签名最多 120 个字符"));
        profile.saveProfile(accountService.nickname(), 2, "", "", "", "");
        QCOMPARE(profile.editorError(), QStringLiteral("资料没有变化"));
        const auto olderRead = AccountServiceTestAccess::profileRequest(accountService, api);
        ApiClient::ProfilePatch patch;
        patch.nickname = QStringLiteral("更新后的昵称");
        patch.gender = 0;
        patch.signature = QString();
        const auto write = ProfileServiceTestAccess::saveRequest(profile, api, patch);
        QSignalSpy saved(&profile, &ProfileService::profileSaved);
        ProfileServiceTestAccess::complete(profile, write, R"({"updated":true})");
        QCOMPARE(saved.size(), 1);
        QCOMPARE(accountService.nickname(), QStringLiteral("更新后的昵称"));
        QCOMPARE(accountService.gender(), 0);
        AccountServiceTestAccess::complete(accountService, olderRead,
                                           R"({"id":900001,"nickname":"写入前的旧昵称"})");
        QCOMPARE(accountService.nickname(), QStringLiteral("更新后的昵称"));
        const auto lateWrite = ProfileServiceTestAccess::saveRequest(profile, api, patch);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900002)));
        ProfileServiceTestAccess::complete(profile, lateWrite, R"({"updated":true})");
        QCOMPARE(saved.size(), 1);
        QCOMPARE(accountService.nickname(), QStringLiteral("Qt 测试账号"));
        QVERIFY(!profile.saving());
        profile.shutdown();
        api.shutdown();
    }
    void liveSameAvatarThroughQtService()
    {
        if (qEnvironmentVariable("ECHO_LIVE_PROFILE_AVATAR") != "1")
            QSKIP("Explicit current-account same-avatar write probe only");
        const std::string input((std::istreambuf_iterator<char>(std::cin)),
                                std::istreambuf_iterator<char>());
        const auto identity = QJsonDocument::fromJson(QByteArray::fromStdString(input)).object();
        DeviceIdentity device;
        device.mid = identity.value("mid").toString();
        device.dfid = identity.value("dfid").toString();
        device.guid = identity.value("guid").toString();
        device.mac = identity.value("mac").toString();
        device.deviceName = identity.value("device_name").toString();
        ApiClient api(device);
        AccountService accountService(api);
        QJsonObject session{{"user_id", identity.value("userid")},
                            {"token", identity.value("token")},
                            {"refresh_credential", identity.value("t1")},
                            {"nickname", ""},
                            {"avatar_url", ""}};
        QVERIFY(AccountServiceTestAccess::adopt(accountService, QJsonDocument(session).toJson()));
        ProfileService profile(api, accountService);
        accountService.reloadProfile();
        QTRY_VERIFY_WITH_TIMEOUT(!accountService.profileLoading(), 30000);
        QVERIFY(accountService.profileDetailAvailable());
        QVERIFY(!accountService.avatarUrl().isEmpty());
        // Download the same public avatar; never send account credentials to
        // the image CDN. The temporary input is removed with this test scope.
        QNetworkAccessManager images;
        QNetworkRequest request{QUrl(accountService.avatarUrl())};
        request.setTransferTimeout(20000);
        std::unique_ptr<QNetworkReply> image(images.get(request));
        QTRY_VERIFY_WITH_TIMEOUT(image->isFinished(), 25000);
        QVERIFY(image->error() == QNetworkReply::NoError);
        const auto bytes = image->readAll();
        QVERIFY(!bytes.isEmpty() && bytes.size() <= 8 * 1024 * 1024);
        QTemporaryDir directory;
        QFile file(directory.filePath("same-current-avatar"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
        file.close();
        QSignalSpy completed(&profile, &ProfileService::avatarFinished);
        profile.uploadAvatar(QUrl::fromLocalFile(file.fileName()));
        QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 60000);
        QVERIFY(completed.first().at(0).toBool());
        QVERIFY(!profile.uploadingAvatar());
        QSignalSpy closed(&api, &ApiClient::closed);
        profile.shutdown();
        api.shutdown();
        QTRY_COMPARE(closed.size(), 1);
    }
    void avatarUploadRejectsInvalidFilesAndIgnoresOldAccountCompletion()
    {
        ApiClient api;
        AccountService accountService(api);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        ProfileService profile(api, accountService);
        QSignalSpy finished(&profile, &ProfileService::avatarFinished);
        profile.uploadAvatar(QUrl("https://example.invalid/avatar.png"));
        QCOMPARE(finished.size(), 1);
        QVERIFY(!profile.uploadingAvatar());
        QTemporaryDir directory;
        QFile invalid(directory.filePath("invalid.png"));
        QVERIFY(invalid.open(QIODevice::WriteOnly));
        invalid.write("not an image");
        invalid.close();
        profile.uploadAvatar(QUrl::fromLocalFile(invalid.fileName()));
        QTRY_COMPARE(finished.size(), 2);
        QVERIFY(!profile.uploadingAvatar());
        QVERIFY(!finished.last().at(0).toBool());
        const auto read = AccountServiceTestAccess::profileRequest(accountService, api);
        const auto upload = ProfileServiceTestAccess::avatarRequest(profile, api);
        ProfileServiceTestAccess::complete(profile, upload,
                                           "{\"avatar_url\":\"http://c1.kgimg.com/v2/kugouicon/"
                                           "sample.jpg\",\"review_pending\":false}");
        QCOMPARE(finished.size(), 3);
        AccountServiceTestAccess::complete(
            accountService, read,
            "{\"id\":900001,\"avatar_url\":\"https://example.invalid/older.jpg\"}");
        QCOMPARE(accountService.avatarUrl(),
                 QString("http://c1.kgimg.com/v2/kugouicon/sample.jpg"));
        const auto stale = ProfileServiceTestAccess::avatarRequest(profile, api);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900002)));
        const auto avatar = accountService.avatarUrl();
        ProfileServiceTestAccess::complete(
            profile, stale,
            "{\"avatar_url\":\"https://example.invalid/stale.jpg\",\"review_pending\":false}");
        QCOMPARE(accountService.avatarUrl(), avatar);
        QCOMPARE(finished.size(), 3);
        api.shutdown();
    }
    void productionCollectionAndSocialEntrypoints()
    {
        Session session;
        QTcpServer sink;
        QVERIFY(sink.listen(QHostAddress::LocalHost));
        connect(&sink, &QTcpServer::newConnection, &sink, [&] {
            while (sink.hasPendingConnections()) {
                auto *socket = sink.nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                    socket->readAll();
                    // Keep the tunnel pending while the test supplies domain acknowledgements.
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        QVERIFY(session.api.setNetworkTransport(sink.serverPort()));
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        session.theme.setMotionEnabled(false);
        PlaybackService playback(session.api, session.preferences, nullptr,
                                 session.directory.filePath("playback.sqlite"));
        SongMetadataService metadata(session.api);
        LibraryCollectionService collections(session.api, session.accounts.playlists,
                                             session.accounts.mutations);
        PlaylistDetailService playlists(
            session.api, playback, session.accounts.playlists, session.accounts.mutations,
            *session.accounts.library.order(), metadata, session.router, false);
        AlbumDetailService albums(session.api, playback, session.router, false);
        ArtistDetailService artists(session.api, playback, session.router, session.shell, false);
        playlists.attachCollections(collections);
        albums.attachCollections(collections);
        artists.attachProfile(session.profile);
        QVERIFY(session.create(&playback));
        const auto cleanup = qScopeGuard([&] {
            session.root.reset();
            session.window = nullptr;
        });
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        UserPlaylistModelTestAccess::present(session.accounts.playlists, session.api, 0);
        ProfileServiceTestAccess::seed(session.profile, 1, {});
        const auto blacklist =
            ContentBlacklistTestAccess::read(*session.profile.blacklist(), session.api, 1);
        ContentBlacklistTestAccess::complete(*session.profile.blacklist(), session.api, blacklist,
                                             R"({"items":[],"total":0,"page":1,"page_size":500})");
        const auto capture = qEnvironmentVariable("ECHO_COLLECTION_SOCIAL_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        const auto click = [&](QQuickItem *item) {
            QVERIFY(item);
            QVERIFY(item->isEnabled());
            QTRY_VERIFY(item->isVisible() && item->width() > 0 && item->height() > 0);
            QTest::qWait(50);
            QSignalSpy pressed(item, SIGNAL(clicked()));
            QVERIFY(pressed.isValid());
            const auto point = item->mapToScene({item->width() / 2, item->height() / 2}).toPoint();
            Test::moveNativePointer(session.window, point);
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            QCOMPARE(pressed.count(), 1);
        };
        QJsonArray songs;
        for (int i = 1; i <= 40; ++i)
            songs.append(QJsonObject{{"id", QString::number(i)},
                                     {"title", QString("Song %1").arg(i)},
                                     {"artists", QJsonArray{}},
                                     {"hash", QString(32, 'a')}});
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark})
            for (const qreal scale : {1., 1.25}) {
                session.theme.setMode(mode);
                session.shell.resetZoom();
                while (session.shell.uiScale() < scale - .001)
                    session.shell.zoomIn();
                for (const auto &name : {QString("playlist-detail"), QString("album-detail"),
                                         QString("artist-detail")}) {
                    QVERIFY(session.router.navigate(name, {},
                                                    name == "playlist-detail" ? "public-playlist"
                                                    : name == "album-detail"  ? "960399"
                                                                              : "3060"));
                    if (name == "playlist-detail") {
                        auto *c = playlists.contextFor(session.router.current());
                        PlaylistDetailContextTestAccess::seed(*c);
                        c->songs()->replaceSongs(songs);
                    } else if (name == "album-detail") {
                        auto *c = albums.contextFor(session.router.current());
                        AlbumDetailContextTestAccess::seed(*c);
                        c->songs()->replaceSongs(songs);
                    } else {
                        auto *c = artists.contextFor(session.router.current());
                        ArtistDetailContextTestAccess::seed(*c);
                        c->songs()->replaceSongs(songs);
                    }
                    QQuickItem *activePage = nullptr;
                    const auto pageName = name == "playlist-detail" ? "playlistDetailPage"
                                          : name == "album-detail"  ? "albumDetailPage"
                                                                    : "artistDetailPage";
                    QTRY_VERIFY((activePage = visual(session.window->contentItem(), pageName)) &&
                                activePage->isVisible());
                    QTRY_VERIFY(visual(activePage, "songDetailHeader"));
                    auto *songView = visual(activePage, "discoverySongs");
                    QVERIFY(songView);
                    songView->setProperty("contentY", -songView->property("topMargin").toReal());
                    const auto buttonName =
                        name == "artist-detail" ? "artistFollowButton" : "detailCollectButton";
                    QQuickItem *button = nullptr;
                    QTRY_VERIFY((button = visual(activePage, buttonName)) && button->isVisible());
                    QVERIFY(button->isEnabled());
                    QTRY_VERIFY(button->width() > 20 && button->height() > 20);
                    QTest::qWait(100);
                    const auto verifyCommand = [&](QQuickItem *target) {
                        qInfo() << "Entry command" << name << target->objectName();
                        click(target);
                        if (name == "artist-detail") {
                            const auto write =
                                ProfileServiceTestAccess::followWrite(session.profile);
                            QVERIFY(write);
                            const auto request = ApiClientTestAccess::request(session.api, write);
                            QCOMPARE(request["operation"].toString(), QString("artist_follow"));
                            QCOMPARE(request["id"].toInteger(), 3060);
                            ProfileServiceTestAccess::fail(session.profile, write,
                                                           R"({"kind":"business"})");
                            session.api.cancel(write);
                        } else {
                            const auto write = PlaylistMutationServiceTestAccess::request(
                                session.accounts.mutations);
                            QVERIFY(write);
                            const auto request = ApiClientTestAccess::request(session.api, write);
                            QCOMPARE(request["operation"].toString(), QString("playlist_add"));
                            QCOMPARE(request["query"]
                                         .toObject()["collection"]
                                         .toObject()["source"]
                                         .toInt(),
                                     name == "album-detail" ? 2 : 1);
                            PlaylistMutationServiceTestAccess::reject(session.accounts.mutations);
                            session.api.cancel(write);
                        }
                    };
                    verifyCommand(button);
                    if (QTest::currentTestFailed())
                        return;
                    if (name == "artist-detail") {
                        click(visual(activePage, "artistBlockButton"));
                        const auto write =
                            ContentBlacklistTestAccess::singerWrite(*session.profile.blacklist());
                        QVERIFY(write);
                        const auto query =
                            ApiClientTestAccess::request(session.api, write)["query"].toObject();
                        QCOMPARE(query["label"].toString(), QString("singer"));
                        QCOMPARE(query["key"].toString(), QString("3060"));
                        ContentBlacklistTestAccess::fail(*session.profile.blacklist(), session.api,
                                                         write);
                    }
                    QTest::qWait(100);
                    if (!capture.isEmpty())
                        QVERIFY(session.window->grabWindow().save(
                            capture +
                            QString("/%1-%2-%3.png").arg(name).arg(int(mode)).arg(scale)));
                    auto *list = visual(activePage, "discoverySongs");
                    QVERIFY(list);
                    list->setProperty("contentY", 500);
                    const auto collapsed = name == "artist-detail" ? "artistFollowCollapsed"
                                                                   : "detailCollectCollapsed";
                    QTRY_VERIFY(visual(activePage, collapsed) &&
                                visual(activePage, collapsed)->isVisible());
                    QTest::qWait(100);
                    verifyCommand(visual(activePage, collapsed));
                    if (QTest::currentTestFailed())
                        return;
                    QTest::qWait(150);
                    if (!capture.isEmpty())
                        QVERIFY(session.window->grabWindow().save(capture +
                                                                  QString("/%1-collapsed-%2-%3.png")
                                                                      .arg(name)
                                                                      .arg(int(mode))
                                                                      .arg(scale)));
                }
                session.router.navigate("profile");
                const auto user = QJsonObject{{"key", "user:77"},
                                              {"user_id", 77},
                                              {"nickname", "中文 Friend"},
                                              {"following", true},
                                              {"mutual", true}};
                ProfileServiceTestAccess::seed(session.profile, 0, {user});
                session.profile.openSocial(ProfileService::SocialTab::Friends);
                auto *page = visual(session.window->contentItem(), "profilePage");
                QTRY_VERIFY(page);
                auto *owner = page->findChild<QObject *>("profileSocialDrawerOwner");
                QVERIFY(owner);
                QVERIFY(QMetaObject::invokeMethod(owner, "open",
                                                  Q_ARG(QVariant, QVariant(QVariantMap{}))));
                QQuickItem *message = nullptr;
                QTRY_VERIFY(
                    (message = visual(session.window->contentItem(), "socialMessageButton")));
                QVERIFY(message->isEnabled());
                auto *follow = visual(session.window->contentItem(), "socialFollowButton");
                QVERIFY(follow && follow->isEnabled());
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/social-%1-%2.png").arg(int(mode)).arg(scale)));
                click(message);
                QTRY_VERIFY(session.profile.chatActive());
                const auto read = ProfileServiceTestAccess::chatRead(session.profile);
                QVERIFY(read);
                ProfileServiceTestAccess::complete(session.profile, read,
                                                   R"({"records":[],"ended":true})");
                QTRY_VERIFY(visual(session.window->contentItem(), "profileChatDraft"));
                auto *draft = visual(session.window->contentItem(), "profileChatDraft");
                draft->setProperty("text", QString("Hello / 你好"));
                auto *send = visual(session.window->contentItem(), "profileChatSend");
                QVERIFY(send && send->isEnabled());
                click(send);
                const auto write = ProfileServiceTestAccess::chatWrite(session.profile);
                QVERIFY(write);
                QVERIFY(!ApiClientTestAccess::automaticVerification(session.api, write));
                ProfileServiceTestAccess::fail(
                    session.profile, write,
                    R"({"kind":"business","message":"Isolated display failure"})");
                QCOMPARE(draft->property("text").toString(), QString("Hello / 你好"));
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/chat-%1-%2.png").arg(int(mode)).arg(scale)));
                QVERIFY(Test::prepareNativeInput(session.window));
                draft->forceActiveFocus(Qt::OtherFocusReason);
                QTRY_VERIFY(draft->hasActiveFocus());
                QTest::keyClick(session.window, Qt::Key_Return);
                const auto accepted = ProfileServiceTestAccess::chatWrite(session.profile);
                QVERIFY(accepted);
                ProfileServiceTestAccess::complete(
                    session.profile, accepted,
                    R"({"msgid":"local-ui-ack","tag":"local-ui-tag","addtime":1792242093})");
                QTRY_COMPARE(draft->property("text").toString(), QString());
                QTRY_COMPARE(session.profile.chatMessages().size(), 1);
                QQuickItem *bubble = nullptr, *meta = nullptr;
                QTRY_VERIFY((bubble = visual(session.window->contentItem(), "profileChatBubble")) &&
                            (meta = visual(session.window->contentItem(), "profileChatMeta")));
                QTest::qWait(100);
                QVERIFY(qAbs(bubble->mapToScene({bubble->width(), 0}).x() -
                             meta->mapToScene({meta->width(), 0}).x()) < .01);
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/chat-ack-%1-%2.png").arg(int(mode)).arg(scale)));
                click(visual(session.window->contentItem(), "profileChatBack"));
                QTRY_VERIFY(!session.profile.chatActive());
                click(visual(session.window->contentItem(), "profileSocialClose"));
                QTRY_VERIFY(!owner->property("item").value<QObject *>());
            }
        for (const auto &warning : warnings)
            qWarning().noquote() << warning[0];
        QCOMPARE(warnings.count(), 0);
    }
    void socialAndDeviceCachesBelongToAccountAndTab()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService accountService(api);
        ProfileService profile(api, accountService);
        QAbstractItemModelTester socialTester(
            profile.socialUsers(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester deviceTester(
            profile.devices(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        const auto friends = ProfileServiceTestAccess::socialRequest(profile, api, 0);
        ProfileServiceTestAccess::complete(profile, friends, socialRows(1000));
        QCOMPARE(profile.socialUsers()->count(), 1000);
        const auto fans = ProfileServiceTestAccess::socialRequest(profile, api, 2);
        profile.openSocial(ProfileService::SocialTab::Fans);
        ProfileServiceTestAccess::complete(profile, fans, socialRows(3));
        QCOMPARE(profile.socialUsers()->count(), 3);
        profile.openSocial(ProfileService::SocialTab::Friends);
        QCOMPARE(profile.socialUsers()->count(), 1000);
        QVERIFY(!profile.socialLoading());
        auto devices = ProfileServiceTestAccess::deviceRequest(profile, api);
        ProfileServiceTestAccess::complete(
            profile, devices,
            R"([{"id":"domain-device","model":"设备","platform":"Android","location":"中国,上海","login_type":1,"active_at_seconds":1700000000,"new_at_seconds":1700000000,"current":true}])");
        QCOMPARE(profile.devices()->count(), 1);
        QVERIFY(profile.devices()
                    ->data(profile.devices()->index(0), LoginDeviceModel::Current)
                    .toBool());
        const auto stale = ProfileServiceTestAccess::socialRequest(profile, api, 0);
        devices = ProfileServiceTestAccess::deviceRequest(profile, api);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900002)));
        ProfileServiceTestAccess::complete(profile, stale, socialRows(200));
        ProfileServiceTestAccess::complete(profile, devices, "[{}]");
        QCOMPARE(profile.socialUsers()->count(), 0);
        QCOMPARE(profile.devices()->count(), 0);
        const auto rejected = ProfileServiceTestAccess::socialRequest(profile, api, 0);
        ProfileServiceTestAccess::fail(profile, rejected, R"({"kind":"business"})");
        QCOMPARE(profile.socialError(), QStringLiteral("列表加载失败，请稍后重试"));
        profile.shutdown();
        api.shutdown();
    }
    void windowSettingsNeverHideWithoutNativeRecovery()
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("window.ini"), QSettings::IniFormat);
        WindowController controller(settings, false);
        QQuickWindow window;
        controller.attach(&window);
        window.show();
        QVERIFY(!controller.trayAvailable());
        controller.setStartMinimized(true);
        controller.applyStartupVisibility();
        QVERIFY(window.isVisible());
        QVERIFY(!controller.background());
        QVERIFY(!controller.systemError().isEmpty());
        controller.setAutoLaunch(true);
        QVERIFY(!controller.autoLaunch());
        QVERIFY(!controller.systemError().isEmpty());
        controller.setHideDockInBackground(true);
        controller.setHideMenuBarInBackground(true);
        settings.sync();
        WindowController reopened(settings, false);
        QVERIFY(reopened.startMinimized());
        QVERIFY(reopened.hideDockInBackground());
        QVERIFY(reopened.hideMenuBarInBackground());
        window.showMaximized();
        controller.showWindow();
        QCOMPARE(window.windowState(), Qt::WindowMaximized);
        // Direct native hide is not the tray/background flow (which stores its
        // state in the controller). Exercise normal hidden/minimized recovery.
        window.showNormal();
        QTRY_COMPARE(window.windowState(), Qt::WindowNoState);
        window.hide();
        controller.showWindow();
        QVERIFY(window.isVisible());
        QCOMPARE(window.windowState(), Qt::WindowNoState);
        window.showMinimized();
        controller.showWindow();
        QVERIFY(window.isVisible());
        QCOMPARE(window.windowState(), Qt::WindowNoState);
        QSignalSpy quit(&controller, &WindowController::quitRequested);
        controller.quit();
        QCOMPARE(quit.size(), 1);
        controller.shutdown();
        QVERIFY(!window.isVisible());
        window.show();
        controller.close();
        QVERIFY(!window.isVisible());
    }
    void startupEntryEscapesBothDesktopParsingLayers()
    {
        const auto text =
            StartupIntegration::desktopEntry(QStringLiteral("/opt/My App/$name`\\\"%/music"));
        const QString expected = QString("Exec=\"/opt/My App/") + QString(2, '\\') + "$name" +
                                 QString(2, '\\') + "`" + QString(6, '\\') + "\"%%/music\"";
        QVERIFY(text.contains(expected));
        QVERIFY(StartupIntegration::desktopEntry("/opt/a=b/music").isEmpty());
        QVERIFY(StartupIntegration::desktopEntry(QString()).isEmpty());
        StartupIntegration unavailable(false);
        QVERIFY(!unavailable.enabled());
        QVERIFY(!unavailable.setEnabled(true).isEmpty());
        QVERIFY(!unavailable.enabled());
    }
    void livePlaylistImportsThroughQtService()
    {
        if (qEnvironmentVariable("ECHO_LIVE_PLAYLIST_IMPORTS") != "1")
            QSKIP("Explicit current-account import and cleanup probe only");
        std::string input((std::istreambuf_iterator<char>(std::cin)),
                          std::istreambuf_iterator<char>());
        const auto identity = QJsonDocument::fromJson(QByteArray::fromStdString(input)).object();
        DeviceIdentity device;
        device.mid = identity.value("mid").toString();
        device.dfid = identity.value("dfid").toString();
        device.guid = identity.value("guid").toString();
        device.mac = identity.value("mac").toString();
        device.deviceName = identity.value("device_name").toString();
        ApiClient api(device);
        QVERIFY(api.setAccount(quint64(identity.value("userid").toInteger()),
                               identity.value("token").toString(),
                               identity.value("t1").toString()));
        UserPlaylistModel source(api);
        PlaylistMutationService mutations(api);
        PlaylistImportService importer(api, source, mutations);
        auto read = [&](const std::function<quint64()> &submit, QJsonObject &result) {
            QSignalSpy ok(&api, &ApiClient::succeeded), failed(&api, &ApiClient::failed);
            const auto id = submit();
            if (!id)
                return false;
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 30000) {
                for (const auto &row : ok)
                    if (row[0].toULongLong() == id) {
                        result = QJsonDocument::fromJson(row[1].toByteArray()).object();
                        return !result.isEmpty();
                    }
                for (const auto &row : failed)
                    if (row[0].toULongLong() == id)
                        return false;
                QTest::qWait(10);
            }
            api.cancel(id);
            return false;
        };
        auto catalog = [&](QJsonArray &rows) {
            rows = {};
            qint64 version = -1, total = -1;
            for (quint32 page = 1; page <= 500; ++page) {
                QJsonObject result;
                if (!read([&] { return api.userPlaylists(page, 200); }, result))
                    return false;
                const auto data = result.value("page").toObject();
                const auto batch = data.value("items").toArray();
                if (page > 1 && (version != result.value("total_revision").toInteger() ||
                                 total != data.value("total").toInteger()))
                    return false;
                version = result.value("total_revision").toInteger();
                total = data.value("total").toInteger();
                for (const auto &row : batch)
                    rows.append(row);
                if (rows.size() == total)
                    return true;
                if (batch.size() < 200)
                    return false;
            }
            return false;
        };
        QJsonArray before;
        QVERIFY(catalog(before));
        QSet<quint64> baseline;
        for (const auto &row : before)
            baseline.insert(quint64(row.toObject().value("cloud_list_id").toInteger()));
        const auto cleanup = [&] {
            QJsonArray rows;
            if (!catalog(rows)) {
                QTest::qFail("Import cleanup catalog unavailable", __FILE__, __LINE__);
                return;
            }
            const auto target = PlaylistImportServiceTestAccess::target(importer);
            QList<quint64> ids;
            for (const auto &value : rows) {
                const auto row = value.toObject();
                const auto id = quint64(row.value("cloud_list_id").toInteger());
                if (baseline.contains(id))
                    continue;
                if (id != target || row.value("kind").toString() != "owned" ||
                    quint64(row.value("creator_id").toInteger()) != api.currentAccountUserId() ||
                    row.value("title").toString() != importer.title()) {
                    QTest::qFail("Import cleanup refused unrelated resource", __FILE__, __LINE__);
                    return;
                }
                ids.append(id);
            }
            for (auto id : ids) {
                QJsonObject deleted;
                read([&] { return api.deletePlaylist({id}); }, deleted);
            }
            if (!catalog(rows)) {
                QTest::qFail("Import cleanup confirmation unavailable", __FILE__, __LINE__);
                return;
            }
            QSet<quint64> after;
            for (const auto &row : rows)
                after.insert(quint64(row.toObject().value("cloud_list_id").toInteger()));
            if (after != baseline)
                QTest::qFail("Import cleanup IDs differ from baseline", __FILE__, __LINE__);
        };
        QFile links(qEnvironmentVariable("ECHO_IMPORT_LINKS"));
        QVERIFY(links.open(QIODevice::ReadOnly));
        const auto inputs = QJsonDocument::fromJson(links.readAll()).array();
        QCOMPARE(inputs.size(), 4);
        for (const auto &input : inputs) {
            auto guard = qScopeGuard(cleanup);
            const auto entry = input.toObject();
            QVERIFY(importer.startLink(entry.value("url").toString()));
            QTRY_VERIFY_WITH_TIMEOUT(!importer.busy(), 180000);
            if (entry.value("provider").toString() == "qishui")
                QCOMPARE(importer.phase(), PlaylistImportService::Failed);
            else {
                QCOMPARE(importer.phase(), PlaylistImportService::Completed);
                QVERIFY(importer.imported() > 0);
            }
            qInfo("Native import terminal: imported=%llu missed=%llu failed=%d",
                  static_cast<unsigned long long>(importer.imported()),
                  static_cast<unsigned long long>(importer.missed()),
                  int(importer.phase() == PlaylistImportService::Failed));
        }
        {
            auto guard = qScopeGuard(cleanup);
            const auto title =
                "EchoMusicNext screenshot native " + QUuid::createUuid().toString(QUuid::Id128);
            QVERIFY(importer.startScreenshot(
                {QUrl::fromLocalFile(qEnvironmentVariable("ECHO_IMPORT_IMAGE"))}, {}, title));
            QTRY_VERIFY_WITH_TIMEOUT(!importer.busy(), 180000);
            QCOMPARE(importer.phase(), PlaylistImportService::Completed);
            QVERIFY(importer.imported() > 0);
            qInfo("Native screenshot import terminal: imported=%llu",
                  static_cast<unsigned long long>(importer.imported()));
        }
        api.shutdown();
    }
    void playlistImportBackgroundResultsStopAndSessionIsolation()
    {
        ApiClient api(DeviceIdentity::temporary());
        api.setAccount(900001, "domain-test-token");
        UserPlaylistModel source(api);
        PlaylistMutationService mutations(api);
        PlaylistImportService importer(api, source, mutations);
        QAbstractItemModelTester tester(&importer,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        QTemporaryDir temp;
        QSettings preferences(temp.filePath("preferences.ini"), QSettings::IniFormat);
        TaskCenterModel tasks;
        PlaylistImportTaskCoordinator coordinator(importer, tasks, preferences);
        QVERIFY(importer.startLink("https://example.invalid/playlist"));
        QCOMPARE(tasks.count(), 1);
        PlaylistImportServiceTestAccess::complete(
            importer, R"({"task_id":71,"user_id":900001,"cloud_list_id":0})");
        QCOMPARE(importer.phase(), PlaylistImportService::Cloud);
        PlaylistImportServiceTestAccess::complete(
            importer,
            R"([{"task_id":71,"user_id":900001,"cloud_list_id":101,"status":3,"title":"领域导入结果","total":3,"imported":2,"missed":1}])");
        PlaylistImportServiceTestAccess::complete(
            importer,
            R"({"task":{"task_id":71,"user_id":900001,"cloud_list_id":101},"page":1,"page_size":100,"missed_tracks":[{"title":"未匹配歌曲","artist":"歌手","reason":"没有结果"}]})");
        QCOMPARE(importer.phase(), PlaylistImportService::Completed);
        QCOMPARE(importer.rowCount(), 1);
        QCOMPARE(importer.imported(), quint64(2));
        QCOMPARE(tasks.data(tasks.index(0), TaskCenterModel::StatusRole).toInt(),
                 int(TaskCenterModel::Completed));
        QSignalSpy reopen(&importer, &PlaylistImportService::showRequested);
        QVERIFY(tasks.invokeAction(
            "playlist-import", tasks.data(tasks.index(0), TaskCenterModel::TokenRole).toString(),
            "view"));
        QCOMPARE(reopen.size(), 1);
        QVERIFY(importer.startLink("https://example.invalid/playlist"));
        PlaylistImportServiceTestAccess::complete(
            importer, R"({"task_id":72,"user_id":900001,"cloud_list_id":0})");
        const auto stopped = PlaylistImportServiceTestAccess::request(importer);
        importer.stop();
        PlaylistImportServiceTestAccess::stale(importer, stopped,
                                               R"([{"task_id":72,"user_id":900001,"status":3}])");
        QCOMPARE(importer.phase(), PlaylistImportService::Stopped);
        QVERIFY(importer.startLink("https://example.invalid/playlist"));
        const auto old = PlaylistImportServiceTestAccess::request(importer);
        api.setAccount(900002, "other-domain-token");
        PlaylistImportServiceTestAccess::stale(importer, old, R"({"task_id":73,"user_id":900001})");
        QCOMPARE(importer.phase(), PlaylistImportService::Idle);
        QCOMPARE(tasks.count(), 0);
        api.shutdown();
    }
    void screenshotTargetSnapshotAndDuplicateConfirmation()
    {
        ApiClient api(DeviceIdentity::temporary());
        api.setAccount(900001, "domain-test-token");
        UserPlaylistModel source(api);
        PlaylistMutationService mutations(api);
        PlaylistImportService importer(api, source, mutations);
        QSignalSpy duplicate(&importer, &PlaylistImportService::duplicateConfirmationRequested);
        PlaylistImportServiceTestAccess::prepareScreenshot(importer, 0, "重复歌单");
        PlaylistImportServiceTestAccess::complete(
            importer,
            R"({"user_id":900001,"total_revision":1,"page":{"page":1,"page_size":200,"total":2,"items":[{"cloud_list_id":1,"kind":"owned","source_code":1,"creator_id":900001,"title":"重复歌单"},{"cloud_list_id":2,"kind":"collected","source_code":1,"creator_id":900003,"title":"他人的歌单"}]}})");
        QCOMPARE(importer.targets().size(), 1);
        QCOMPARE(duplicate.size(), 1);
        QCOMPARE(importer.duplicateName(), QString("重复歌单"));
        importer.confirmDuplicate(false, {});
        QCOMPARE(importer.phase(), PlaylistImportService::Stopped);
        PlaylistImportServiceTestAccess::prepareScreenshot(importer, 2, {});
        PlaylistImportServiceTestAccess::complete(
            importer,
            R"({"user_id":900001,"total_revision":1,"page":{"page":1,"page_size":200,"total":1,"items":[{"cloud_list_id":2,"kind":"collected","source_code":1,"creator_id":900003,"title":"他人的歌单"}]}})");
        QCOMPARE(importer.phase(), PlaylistImportService::Failed);
        QVERIFY(importer.errorMessage().contains("目标歌单"));
        api.shutdown();
    }
    void productionSidebarImportEntranceAndModes()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        const auto click = [&session](const char *name) {
            auto *item = visual(session.window->contentItem(), QString::fromLatin1(name));
            if (!item)
                return false;
            QTest::mouseClick(
                session.window, Qt::LeftButton, Qt::NoModifier,
                item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
            return true;
        };
        QVERIFY(click("sidebarPlaylistAdd"));
        auto *menu = session.root->findChild<QObject *>("sidebarPlaylistAddMenu");
        QVERIFY(menu);
        QTRY_VERIFY(menu->property("opened").toBool());
        const auto capture = qEnvironmentVariable("ECHO_SIDEBAR_ORDER_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1., 1.25}) {
                menu->setProperty("uiScale", scale);
                QTest::qWait(100);
                QList<qreal> leftEdges;
                for (const auto *name : {"sidebarPlaylistNew", "sidebarPlaylistImport"}) {
                    auto *row = visual(session.window->contentItem(), QString::fromLatin1(name));
                    QVERIFY(row);
                    auto *icon = visual(row, "sidebarPlaylistActionIcon");
                    auto *title = visual(row, "sidebarPlaylistActionTitle");
                    auto *description = visual(row, "sidebarPlaylistActionDescription");
                    QVERIFY(icon && title && description);
                    const auto titleLeft = title->mapToScene(QPointF()).x();
                    const auto descriptionLeft = description->mapToScene(QPointF()).x();
                    QVERIFY(qAbs(titleLeft - descriptionLeft) < .5);
                    leftEdges.append(titleLeft);
                    const auto textCenter =
                        (title->mapToScene(QPointF()).y() +
                         description->mapToScene(QPointF(0, description->height())).y()) /
                        2;
                    const auto iconCenter = icon->mapToScene(QPointF(0, icon->height() / 2)).y();
                    QVERIFY2(qAbs(textCenter - iconCenter) < .5,
                             qPrintable(QString("%1 text=%2 icon=%3 title=%4 description=%5")
                                            .arg(name)
                                            .arg(textCenter)
                                            .arg(iconCenter)
                                            .arg(title->height())
                                            .arg(description->height())));
                }
                QVERIFY(qAbs(leftEdges[0] - leftEdges[1]) < .5);
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/playlist-add-%1-%2.png")
                                      .arg(theme == ThemeStore::Light ? "light" : "dark")
                                      .arg(scale)));
            }
        }
        menu->setProperty("uiScale", 1.);
        QVERIFY(click("sidebarPlaylistImport"));
        auto *dialog = session.root->findChild<QObject *>("playlistImportDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *link = visual(session.window->contentItem(), "playlistImportLink");
        QVERIFY(link);
        auto *start = visual(session.window->contentItem(), "playlistImportStart");
        QVERIFY(start);
        QVERIFY(!start->isEnabled());
        dialog->setProperty("linkText", "https://example.invalid/playlist");
        QTRY_VERIFY(start->isEnabled());
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (int mode = 0; mode < 2; ++mode) {
                dialog->setProperty("mode", mode);
                QTest::qWait(100);
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/import-%1-%2.png")
                                      .arg(theme == ThemeStore::Light ? "light" : "dark")
                                      .arg(mode)));
            }
        }
        // Use the production FileDialog accepted handler without uploading or
        // creating a playlist. The discarded tenth file must not be validated.
        QTemporaryDir screenshots;
        QList<QUrl> selectedFiles;
        QImage screenshot(16, 16, QImage::Format_RGB32);
        screenshot.fill(Qt::white);
        for (int i = 0; i < 9; ++i) {
            const auto path = screenshots.filePath(QString("shot-%1.png").arg(i));
            QVERIFY(screenshot.save(path));
            selectedFiles.append(QUrl::fromLocalFile(path));
        }
        const auto oversizedPath = screenshots.filePath("oversized.png");
        QFile oversized(oversizedPath);
        QVERIFY(oversized.open(QIODevice::WriteOnly));
        QVERIFY(oversized.resize(10 * 1024 * 1024 + 1));
        oversized.close();
        selectedFiles.append(QUrl::fromLocalFile(oversizedPath));
        dialog->setProperty("mode", 1);
        dialog->setProperty("targetMode", 1);
        dialog->setProperty("newName", "截图选择回归");
        const auto selectFiles = [&](const QList<QUrl> &files) {
            return QMetaObject::invokeMethod(dialog, "selectScreenshots",
                                             Q_ARG(QVariant, QVariant::fromValue(files)));
        };
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1., 1.25}) {
                dialog->setProperty("uiScale", scale);
                QVERIFY(selectFiles(selectedFiles));
                QCOMPARE(dialog->property("files").value<QList<QUrl>>(), selectedFiles.mid(0, 9));
                auto *selectionMessage =
                    visual(session.window->contentItem(), "playlistImportInputError");
                QVERIFY(selectionMessage);
                QTRY_VERIFY(selectionMessage->isVisible());
                QCOMPARE(selectionMessage->property("text").toString(),
                         QString("一次最多选择 9 张截图"));
                QVERIFY(start->isEnabled());
                QTest::qWait(100);
                if (!capture.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/import-selection-%1-%2.png")
                                      .arg(theme == ThemeStore::Light ? "light" : "dark")
                                      .arg(scale)));
                QVERIFY(selectFiles({selectedFiles.last(), selectedFiles.first()}));
                QVERIFY(dialog->property("files").value<QList<QUrl>>().isEmpty());
                QTRY_VERIFY(selectionMessage->isVisible());
                QCOMPARE(selectionMessage->property("text").toString(),
                         QString("oversized.png 超过 10 MB"));
                QVERIFY(!start->isEnabled());
                QVERIFY(selectFiles({selectedFiles.first()}));
                QCOMPARE(dialog->property("files").value<QList<QUrl>>().size(), 1);
                QTRY_VERIFY(!selectionMessage->isVisible());
                QVERIFY(start->isEnabled());
            }
        }
        dialog->setProperty("uiScale", 1.);
        dialog->setProperty("mode", 0);
        session.theme.setMode(ThemeStore::Light);
        QTest::mouseClick(
            session.window, Qt::LeftButton, Qt::NoModifier,
            start->mapToScene(QPointF(start->width() / 2, start->height() / 2)).toPoint());
        auto &importer = *session.accounts.library.importer();
        QVERIFY(importer.busy());
        PlaylistImportServiceTestAccess::complete(
            importer, R"({"task_id":71,"user_id":900001,"cloud_list_id":0})");
        PlaylistImportServiceTestAccess::complete(
            importer,
            R"([{"task_id":71,"user_id":900001,"cloud_list_id":101,"status":2,"title":"领域导入展示","total":3,"imported":1,"missed":0}])");
        PlaylistImportServiceTestAccess::pausePoll(importer);
        QTest::qWait(100);
        if (!capture.isEmpty())
            QVERIFY(session.window->grabWindow().save(capture + "/import-progress-light.png"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("opened").toBool());
        QVERIFY(importer.busy());
        importer.reopen();
        QTRY_VERIFY(dialog->property("opened").toBool());
        PlaylistImportServiceTestAccess::poll(importer);
        PlaylistImportServiceTestAccess::complete(
            importer,
            R"([{"task_id":71,"user_id":900001,"cloud_list_id":101,"status":3,"title":"领域导入展示","total":3,"imported":2,"missed":1}])");
        PlaylistImportServiceTestAccess::complete(
            importer,
            R"({"task":{"task_id":71,"user_id":900001,"cloud_list_id":101},"page":1,"page_size":100,"missed_tracks":[{"title":"未匹配展示歌曲","artist":"歌手","reason":"没有结果"}]})");
        QCOMPARE(importer.phase(), PlaylistImportService::Completed);
        QTest::qWait(100);
        auto *results = visual(session.window->contentItem(), "playlistImportMissed");
        QVERIFY(results);
        QVERIFY(results->height() >= 4 * 58);
        QCOMPARE(results->property("count").toInt(), 1);
        if (!capture.isEmpty())
            QVERIFY(session.window->grabWindow().save(capture + "/import-result-light.png"));
        importer.dismiss();
        QVERIFY(importer.startLink("https://example.invalid/playlist"));
        PlaylistImportServiceTestAccess::complete(
            importer, R"({"task_id":72,"user_id":900001,"cloud_list_id":0})");
        PlaylistImportServiceTestAccess::complete(
            importer,
            R"([{"task_id":72,"user_id":900001,"cloud_list_id":0,"status":11,"title":"","total":0,"imported":0,"missed":0}])");
        QCOMPARE(importer.phase(), PlaylistImportService::Failed);
        QCOMPARE(importer.stages().size(), 1);
        auto *warningImage = visual(session.window->contentItem(), "playlistImportSummaryIcon");
        QVERIFY(warningImage);
        QTRY_COMPARE(warningImage->property("status").toInt(), 1);
        QTest::qWait(100);
        if (!capture.isEmpty())
            QVERIFY(session.window->grabWindow().save(capture + "/import-failed-light.png"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("opened").toBool());
        QCOMPARE(warnings.size(), 0);
    }
    void incompleteAndRepeatedLibraryIdsKeepEveryRow()
    {
        ApiClient api(DeviceIdentity::temporary());
        api.setAccount(900001, "domain-test-token");
        UserPlaylistModel library(api);
        UserPlaylistModelTestAccess::complete(
            library, api,
            R"({"total_revision":12,"page":{"total":null,"items":[{"cloud_list_id":2,"title":"A"},{"cloud_list_id":2,"title":"B"},{"cloud_list_id":null,"title":"资料待补齐"}]}})");
        QCOMPARE(library.rowCount(), 3);
        QCOMPARE(library.data(library.index(1), UserPlaylistModel::TitleRole).toString(),
                 QString("B"));
        QCOMPARE(library.data(library.index(2), UserPlaylistModel::TitleRole).toString(),
                 QString("资料待补齐"));
        QVERIFY(!library.libraryIdAt(2));
        QVERIFY(!library.cloudIdAt(2));
        QVERIFY(library.errorMessage().isEmpty());
        api.shutdown();
    }
    void libraryConsumersAcceptPageEchoAndCountVariations()
    {
        ApiClient api(DeviceIdentity::temporary());
        api.setAccount(900001, "domain-test-token");
        UserPlaylistModel library(api);
        PlaylistMutationService mutations(api);
        PlaylistOrderModel order(api, mutations);
        PlaylistImportService importer(api, library, mutations);
        const QByteArray snapshot =
            R"({"user_id":900001,"total_revision":12,"page":{"page":999,"page_size":1,"total":0,"items":[
            {"cloud_list_id":2,"kind":"owned","title":"A","sort_index":0,"song_count":2,"source_code":1,"creator_id":900001},
            {"cloud_list_id":3,"kind":"owned","title":"B","sort_index":1,"song_count":3,"source_code":1,"creator_id":900001}]}})";
        order.open(0);
        PlaylistOrderModelTestAccess::complete(order, PlaylistOrderModelTestAccess::request(order),
                                               snapshot);
        QVERIFY(order.errorMessage().isEmpty());
        QCOMPARE(order.rowCount(), 2);
        QCOMPARE(PlaylistOrderModelTestAccess::positions(order).size(), 2);
        importer.refreshTargets();
        PlaylistImportServiceTestAccess::complete(importer, snapshot);
        QVERIFY(importer.errorMessage().isEmpty());
        QCOMPARE(importer.targets().size(), 2);
        auto repeated = QJsonDocument::fromJson(snapshot).object();
        auto page = repeated["page"].toObject();
        page["total"] = 999;
        repeated["page"] = page;
        const auto payload = QJsonDocument(repeated).toJson();
        order.open(0);
        PlaylistOrderModelTestAccess::complete(order, PlaylistOrderModelTestAccess::request(order),
                                               payload);
        QVERIFY(order.busy());
        PlaylistOrderModelTestAccess::complete(order, PlaylistOrderModelTestAccess::request(order),
                                               payload);
        QVERIFY(!order.busy());
        QVERIFY(order.errorMessage().isEmpty());
        QCOMPARE(order.rowCount(), 2);
        importer.refreshTargets();
        PlaylistImportServiceTestAccess::complete(importer, payload);
        QVERIFY(importer.loadingTargets());
        PlaylistImportServiceTestAccess::complete(importer, payload);
        QVERIFY(!importer.loadingTargets());
        QVERIFY(importer.errorMessage().isEmpty());
        QCOMPARE(importer.targets().size(), 2);

        api.shutdown();
    }
    void playlistOrderKeepsFixedSlotsAndRejectsChangedVersions()
    {
        ApiClient api(DeviceIdentity::temporary());
        api.setAccount(900001, "domain-test-token");
        PlaylistMutationService mutations(api);
        PlaylistOrderModel model(api, mutations);
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        const QByteArray snapshot =
            R"({"user_id":900001,"total_revision":12,"page":{"page":1,"page_size":200,"total":6,"items":[
          {"cloud_list_id":1,"kind":"owned","title":"默认收藏","sort_index":0,"song_count":1,"source_code":1,"default_kind":1},
          {"cloud_list_id":2,"kind":"owned","title":"A","sort_index":1,"song_count":2,"source_code":1,"default_kind":null},
          {"cloud_list_id":3,"kind":"owned","title":"我喜欢","sort_index":2,"song_count":3,"source_code":1,"default_kind":2},
          {"cloud_list_id":4,"kind":"owned","title":"B","sort_index":3,"song_count":4,"source_code":1,"default_kind":null},
          {"cloud_list_id":5,"kind":"collected","title":"专辑","sort_index":0,"song_count":5,"source_code":2,"default_kind":null},
          {"cloud_list_id":6,"kind":"collected","title":"收藏歌单","sort_index":1,"song_count":6,"source_code":1,"default_kind":null}
        ]}})";
        model.open(0);
        QVERIFY(model.busy());
        PlaylistOrderModelTestAccess::complete(model, PlaylistOrderModelTestAccess::request(model),
                                               snapshot);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), PlaylistOrderModel::Title).toString(), QString("A"));
        model.move(0, 1);
        QVERIFY(model.dirty());
        const auto positions = PlaylistOrderModelTestAccess::positions(model);
        QCOMPARE(positions.size(), 4);
        QCOMPARE(positions[0].id.value, quint64(1));
        QCOMPARE(positions[1].id.value, quint64(4));
        QCOMPARE(positions[2].id.value, quint64(3));
        QCOMPARE(positions[3].id.value, quint64(2));
        QCOMPARE(positions[3].position, quint32(3));
        model.save();
        PlaylistOrderModelTestAccess::complete(
            model, PlaylistOrderModelTestAccess::request(model),
            R"({"user_id":900001,"total_revision":13,"page":{"total":6}})");
        QVERIFY(!model.saving());
        QVERIFY(model.errorMessage().contains(QStringLiteral("其他位置更新")));
        QVERIFY(model.dirty());
        model.restore();
        QVERIFY(!model.dirty());
        model.open(1);
        PlaylistOrderModelTestAccess::complete(model, PlaylistOrderModelTestAccess::request(model),
                                               snapshot);
        QCOMPARE(model.rowCount(), 1);
        const auto collected = PlaylistOrderModelTestAccess::positions(model);
        QCOMPARE(collected.size(), 2);
        QCOMPARE(collected[0].id.value, quint64(5));
        QCOMPARE(collected[1].id.value, quint64(6));
        model.open(0);
        const auto stale = PlaylistOrderModelTestAccess::request(model);
        api.setAccount(900002, "different-domain-token");
        PlaylistOrderModelTestAccess::complete(model, stale, snapshot);
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.busy());
        api.shutdown();
    }
    void productionSidebarOrderEditor()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        auto *button = visual(session.window->contentItem(), "sidebarPlaylistOrder");
        QVERIFY(button && button->isEnabled());
        QTest::mouseClick(
            session.window, Qt::LeftButton, Qt::NoModifier,
            button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint());
        auto *dialog = session.root->findChild<QObject *>("playlistOrderDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto &model = *session.accounts.library.order();
        QJsonArray items;
        for (int i = 0; i < 1000; ++i)
            items.append(QJsonObject{{"cloud_list_id", i + 1},
                                     {"kind", "owned"},
                                     {"title", QString("歌单 %1").arg(i + 1)},
                                     {"sort_index", i},
                                     {"song_count", i},
                                     {"source_code", 1},
                                     {"default_kind", QJsonValue::Null}});
        for (int page = 1; page <= 5; ++page) {
            QJsonArray chunk;
            for (int i = (page - 1) * 200; i < page * 200; ++i)
                chunk.append(items[i]);
            PlaylistOrderModelTestAccess::complete(
                model, PlaylistOrderModelTestAccess::request(model),
                QJsonDocument(QJsonObject{{"user_id", 900001},
                                          {"total_revision", 20},
                                          {"page", QJsonObject{{"page", page},
                                                               {"page_size", 200},
                                                               {"total", 1000},
                                                               {"items", chunk}}}})
                    .toJson());
        }
        QTRY_COMPARE(model.rowCount(), 1000);
        auto *list = visual(session.window->contentItem(), "playlistOrderList");
        QVERIFY(list);
        QTRY_VERIFY(visualCount(list, "playlistOrderRow") > 0);
        QVERIFY(visualCount(list, "playlistOrderRow") < 40);
        auto *position = visual(session.window->contentItem(), "playlistOrderPosition");
        QVERIFY(position);
        model.select(4);
        QTRY_COMPARE(position->property("value").toInt(), 5);
        auto *number = visual(position, "numberInputField");
        QVERIFY(number);
        number->setProperty("text", "7");
        QVERIFY(QMetaObject::invokeMethod(position, "commit"));
        QCOMPARE(position->property("value").toInt(), 7);
        QCOMPARE(model.selected(), 4);
        model.select(20);
        QTRY_COMPARE(position->property("value").toInt(), 21);
        QTRY_COMPARE(number->property("text").toString(), QString("21"));
        model.select(0);
        const auto from = list->mapToScene(QPointF(100, 26)).toPoint();
        const auto to = list->mapToScene(QPointF(100, 182)).toPoint();
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, from);
        QTest::mouseMove(session.window, from + QPoint(0, 15), 20);
        QTest::mouseMove(session.window, to, 30);
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, to);
        QTRY_COMPARE(model.data(model.index(3), PlaylistOrderModel::Title).toString(),
                     QString("歌单 1"));
        model.restore();

        const auto capture = qEnvironmentVariable("ECHO_SIDEBAR_ORDER_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            QVERIFY(QDir().mkpath(capture));
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                session.theme.setMode(mode);
                QTest::qWait(120);
                QVERIFY(session.window->grabWindow().save(capture + (mode == ThemeStore::Light
                                                                         ? "/order-light.png"
                                                                         : "/order-dark.png")));
            }
        }
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("opened").toBool());
        QTRY_COMPARE(model.rowCount(), 0);
        QCOMPARE(warnings.size(), 0);
    }
    void sidebarLayoutPersistsAndKeepsGroupsAndOneShortcut()
    {
        QTemporaryDir temp;
        QSettings preferences(temp.filePath("preferences.ini"), QSettings::IniFormat);
        ShellSettings shell(preferences);
        auto &layout = *shell.sidebarLayout();
        QAbstractItemModelTester tester(&layout,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester cards(layout.shortcutModel(),
                                       QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(layout.rowCount(), 3);
        QCOMPARE(layout.shortcutCatalog().size(), 9);
        for (const auto &value : layout.shortcutCatalog()) {
            const auto entry = value.toMap();
            if (entry["key"] == "recommend")
                QCOMPARE(entry["route"].toString(), QString("recommend-songs"));
        }
        QCOMPARE(layout.sections()[0].toMap()["title"].toString(), QStringLiteral("音乐菜单"));
        QCOMPARE(layout.sections()[1].toMap()["title"].toString(), QStringLiteral("歌单"));
        QCOMPARE(layout.shortcuts().size(), 2);
        layout.toggleHidden("favorites");
        layout.toggleGroup("library");
        layout.toggleGroup("playlists");
        QVERIFY(!layout.menuVisible());
        QVERIFY(!layout.playlistAreaVisible());
        layout.moveBefore("history", "favorites");
        QCOMPARE(layout.data(layout.index(0), SidebarLayoutModel::Key).toString(),
                 QString("history"));
        layout.moveBefore("home", "favorites"); // Card catalog is not a menu group.
        QCOMPARE(layout.rowCount(), 3);
        layout.moveShortcutBefore("explore", "home");
        QCOMPARE(layout.shortcuts()[0].toMap()["key"].toString(), QString("explore"));
        layout.toggleShortcut("home");
        layout.toggleShortcut("explore"); // Must retain one card.
        layout.toggleShortcut("invalid");
        QCOMPARE(layout.shortcuts().size(), 1);
        preferences.sync();
        QSettings reread(temp.filePath("preferences.ini"), QSettings::IniFormat);
        ShellSettings restored(reread);
        auto *loaded = restored.sidebarLayout();
        QCOMPARE(loaded->data(loaded->index(0), SidebarLayoutModel::Key).toString(),
                 QString("history"));
        QVERIFY(loaded->data(loaded->index(1), SidebarLayoutModel::Hidden).toBool());
        QVERIFY(loaded->sections()[0].toMap()["hidden"].toBool());
        QVERIFY(loaded->sections()[1].toMap()["hidden"].toBool());
        QCOMPARE(loaded->shortcuts().size(), 1);
        QCOMPARE(loaded->shortcuts()[0].toMap()["key"].toString(), QString("explore"));
        loaded->restoreDefaults();
        QCOMPARE(loaded->shortcuts().size(), 1);
        QVERIFY(loaded->playlistAreaVisible());
        QVERIFY(loaded->menuVisible());
        QCOMPARE(loaded->data(loaded->index(0), SidebarLayoutModel::Key).toString(),
                 QString("favorites"));
        loaded->restoreShortcuts();
        QCOMPARE(loaded->shortcuts().size(), 2);
        QVERIFY(!loaded->data(loaded->index(0), SidebarLayoutModel::Hidden).toBool());
    }
    void sidebarHomeAndDragTransaction()
    {
        QTemporaryDir temp;
        const auto path = temp.filePath("preferences.ini");
        QSettings preferences(path, QSettings::IniFormat);
        preferences.setValue("sidebar/shortcuts", QStringList{"recommend", "explore", "home"});
        SidebarLayoutModel layout(preferences, nullptr);
        QAbstractItemModelTester cards(layout.shortcutModel(),
                                       QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(layout.homeRoute(), QString("recommend-songs"));
        QVERIFY(layout.beginShortcutDrag("home"));
        layout.previewShortcutMove("home", "recommend");
        QCOMPARE(layout.homeRoute(), QString("home"));
        // A preview must neither write the temporary order nor reset delegates.
        QCOMPARE(preferences.value("sidebar/shortcuts").toStringList().first(),
                 QString("recommend"));
        layout.toggleHidden("favorites"); // Another layout write must not commit this drag.
        QCOMPARE(preferences.value("sidebar/shortcuts").toStringList().first(),
                 QString("recommend"));
        layout.endShortcutDrag(false);
        QCOMPARE(layout.homeRoute(), QString("recommend-songs"));
        QVERIFY(layout.beginShortcutDrag("explore"));
        QVERIFY(!layout.beginShortcutDrag("home"));
        layout.previewShortcutMove("home", "recommend"); // Foreign gesture ignored.
        QCOMPARE(layout.homeRoute(), QString("recommend-songs"));
        layout.previewShortcutMove("explore", "recommend");
        layout.endShortcutDrag(true);
        preferences.sync();
        QSettings restoredPreferences(path, QSettings::IniFormat);
        SidebarLayoutModel restored(restoredPreferences, nullptr);
        QCOMPARE(restored.homeRoute(), QString("explore"));
        RouteController router;
        QVERIFY(router.registerPage(
            {"explore", "/main/explore", QUrl("qrc:/qt/qml/EchoMusic/Discovery/ExplorePage.qml")}));
        QVERIFY(router.navigate(restored.homeRoute()));
        QCOMPARE(router.current()->name(), QString("explore"));
        QVERIFY(!router.canGoBack()); // No unwanted recommendation page in startup history.
        const auto guestPage = router.current()->instanceId();
        router.invalidateAccountPreservingRoute();
        QCOMPARE(router.current()->name(), QString("explore"));
        QVERIFY(router.current()->instanceId() != guestPage);
        QVERIFY(!router.canGoBack());
        QCOMPARE(router.mountedCount(), 1);
    }
    void sidebarShortcutHoverDragAcrossThemesAndScales_data()
    {
        QTest::addColumn<int>("themeMode");
        QTest::addColumn<qreal>("scale");
        QTest::newRow("light-100") << int(ThemeStore::Light) << qreal(1.);
        QTest::newRow("light-125") << int(ThemeStore::Light) << qreal(1.25);
        QTest::newRow("dark-100") << int(ThemeStore::Dark) << qreal(1.);
        QTest::newRow("dark-125") << int(ThemeStore::Dark) << qreal(1.25);
    }
    void sidebarShortcutHoverDragAcrossThemesAndScales()
    {
        QFETCH(int, themeMode);
        QFETCH(qreal, scale);
        const auto mode = ThemeStore::Mode(themeMode);
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        // Sidebar targets need real registered providers; the test only supplies
        // page registration, never fabricated protocol responses.
        for (const auto &name : {"home", "explore", "ranking"})
            QVERIFY(session.router.registerPage(
                {QString::fromLatin1(name), "/main/" + QString::fromLatin1(name),
                 QUrl("qrc:/qt/qml/EchoMusic/Discovery/ExplorePage.qml")}));
        session.theme.setMode(mode);
        if (scale > 1.)
            for (int i = 0; i < 5; ++i)
                session.shell.zoomIn();
        // Match Application: restored scale and providers precede QML creation.
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *layout = session.shell.sidebarLayout();
        const auto item = [&session](const QString &name) {
            return visual(session.window->contentItem(), name);
        };
        const auto center = [](QQuickItem *target) {
            return target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
        };
        const auto capture = [&session](const QString &name) {
            const auto directory = qEnvironmentVariable("ECHO_SIDEBAR_COMPLETE_CAPTURE_DIR");
            if (directory.isEmpty())
                return true;
            QDir().mkpath(directory);
            session.window->update();
            return session.window->grabWindow().save(directory + "/" + name + ".png");
        };
        layout->restoreShortcuts();
        Test::moveNativePointer(session.window, QPoint(800, 50));
        if (QGuiApplication::platformName() == "cocoa")
            QTRY_COMPARE(QCursor::pos(), session.window->mapToGlobal(QPoint(800, 50)));
        QTest::qWait(220);
        auto *home = item("sidebarShortcut-home");
        auto *explore = item("sidebarShortcut-explore");
        QVERIFY(home && explore);
        auto *label = visual(home, "sidebarShortcutLabel");
        auto *icon = visual(home, "sidebarShortcutIcon");
        auto *remove = item("sidebarShortcutRemove-home");
        QVERIFY(label && icon && remove);
        QTRY_COMPARE(icon->opacity(), 1.);
        QCOMPARE(label->opacity(), 0.);
        QCOMPARE(icon->width(), 20.);
        QCOMPARE(remove->width(), 24.);
        bool iconFaded = false, labelFaded = false;
        const auto iconFade = QObject::connect(icon, &QQuickItem::opacityChanged, home, [&] {
            iconFaded |= icon->opacity() > 0 && icon->opacity() < 1;
        });
        const auto labelFade = QObject::connect(label, &QQuickItem::opacityChanged, home, [&] {
            labelFaded |= label->opacity() > 0 && label->opacity() < 1;
        });
        const auto stopFade = qScopeGuard([&] {
            QObject::disconnect(iconFade);
            QObject::disconnect(labelFade);
        });
        Test::moveNativePointer(session.window, center(home));
        if (QGuiApplication::platformName() == "cocoa")
            QTRY_COMPARE(QCursor::pos(), session.window->mapToGlobal(center(home)));
        QTRY_COMPARE(label->opacity(), 1.);
        QTRY_COMPARE(icon->opacity(), 0.);
        QVERIFY(iconFaded && labelFaded);
        QCOMPARE(label->width(), home->width() - 16 - 24);
        QVERIFY(capture(QString("shortcut-hover-%1-%2").arg(int(mode)).arg(scale)));
        // Pointer removal does not send keyboard focus to the survivor.
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, center(remove));
        QTRY_COMPARE(layout->shortcuts().size(), 1);
        QVERIFY(!item("sidebarShortcut-explore")
                     ->property("activation")
                     .value<QObject *>()
                     ->property("activeFocus")
                     .toBool());
        layout->restoreShortcuts();
        QTest::qWait(200);
        home = item("sidebarShortcut-home");
        explore = item("sidebarShortcut-explore");
        QPointer<QQuickItem> kept(home);
        const auto oldX = home->x();
        const auto targetX = explore->x();
        bool animatedBetweenPositions = false;
        const auto movement = QObject::connect(home, &QQuickItem::xChanged, home, [&] {
            animatedBetweenPositions |= home->x() > oldX && home->x() < targetX;
        });
        const auto stopMovement = qScopeGuard([&] { QObject::disconnect(movement); });
        const auto start = center(home), end = center(explore);
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(session.window, start + QPoint(10, 0), 15);
        QTest::mouseMove(session.window, end, 20);
        // Reorder is already visible before the mouse is released.
        QTRY_COMPARE(layout->homeRoute(), QString("explore"));
        QTest::mouseMove(session.window, end + QPoint(1, 0), 1);
        QCOMPARE(layout->homeRoute(), QString("explore"));
        QTest::qWait(35);
        QVERIFY(capture(QString("shortcut-drag-%1-%2").arg(int(mode)).arg(scale)));
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, end);
        QTRY_COMPARE(home->x(), targetX);
        QVERIFY(animatedBetweenPositions);
        QCOMPARE(item("sidebarShortcut-home"), kept.data());
        QCOMPARE(session.router.current()->name(), QString("profile"));
        Test::moveNativePointer(session.window, QPoint(800, 50));
        QTest::qWait(220);
        QVERIFY(capture(QString("shortcut-rest-%1-%2").arg(int(mode)).arg(scale)));
    }
    void sidebarShortcutHoverDragAndFocus()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        // Sidebar targets need real registered providers; the test only supplies
        // page registration, never fabricated protocol responses.
        for (const auto &name : {"home", "explore", "ranking"})
            QVERIFY(session.router.registerPage(
                {QString::fromLatin1(name), "/main/" + QString::fromLatin1(name),
                 QUrl("qrc:/qt/qml/EchoMusic/Discovery/ExplorePage.qml")}));
        // Match Application: providers are registered before the first QML binding.
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *layout = session.shell.sidebarLayout();
        const auto item = [&session](const QString &name) {
            return visual(session.window->contentItem(), name);
        };
        const auto center = [](QQuickItem *target) {
            return target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
        };
        const auto capture = [&session](const QString &name) {
            const auto directory = qEnvironmentVariable("ECHO_SIDEBAR_COMPLETE_CAPTURE_DIR");
            if (directory.isEmpty())
                return true;
            QDir().mkpath(directory);
            session.window->update();
            return session.window->grabWindow().save(directory + "/" + name + ".png");
        };
        // Escape cancels a real preview, including both display and preferences.
        layout->restoreShortcuts();
        QTest::qWait(220);
        auto *cancelHome = item("sidebarShortcut-home");
        const auto cancelStart = center(cancelHome);
        const auto cancelEnd = center(item("sidebarShortcut-explore"));
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, cancelStart);
        QTest::mouseMove(session.window, cancelStart + QPoint(10, 0), 15);
        QTest::mouseMove(session.window, cancelEnd, 20);
        QTRY_COMPARE(layout->homeRoute(), QString("explore"));
        QTest::keyClick(session.window, Qt::Key_Escape);
        QTRY_COMPARE(layout->homeRoute(), QString("home"));
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, cancelEnd);
        QCOMPARE(session.preferences.value("sidebar/shortcuts").toStringList().first(),
                 QString("home"));
        QCOMPARE(session.router.current()->name(), QString("profile"));
        session.shell.resetZoom();
        layout->restoreShortcuts();
        QTest::qWait(200);
        auto *home = item("sidebarShortcut-home");
        auto *activate = home->property("activation").value<QQuickItem *>();
        QVERIFY(activate);
        QVERIFY(activate->isEnabled());
        QVERIFY(Test::prepareNativeInput(session.window));
        item("sidebarShortcutAdd")->forceActiveFocus(Qt::TabFocusReason);
        activate->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(activate->hasActiveFocus());
        QTest::keyClick(session.window, Qt::Key_Right, Qt::AltModifier);
        QTRY_COMPARE(layout->homeRoute(), QString("explore"));
        QTRY_VERIFY(activate->hasActiveFocus());
        // Keyboard removal continues at the surviving card, without scrolling.
        auto *remove = item("sidebarShortcutRemove-home");
        // Respect macOS's system Tab-text-only preference; direct focus also
        // exercises the accessibility/keyboard activation path on every platform.
        remove->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(remove->hasActiveFocus());
        QTest::keyClick(session.window, Qt::Key_Space);
        QTRY_COMPARE(layout->shortcuts().size(), 1);
        auto *survivor =
            item("sidebarShortcut-explore")->property("activation").value<QQuickItem *>();
        QTRY_VERIFY(survivor->hasActiveFocus());
        QVERIFY(!item("sidebarShortcutRemove-explore")->isVisible());
        // The same Alt+arrow conflict also affected the shared shell action.
        auto *tasks = item("titlebar-action-tasks");
        QVERIFY(tasks && tasks->isEnabled());
        tasks->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Left, Qt::AltModifier);
        QTRY_COMPARE(session.preferences.value("shell/titlebar/order").toStringList().value(0),
                     QString("host/tasks"));
        QCOMPARE(session.router.current()->name(), QString("profile"));
        // Reduced motion uses the same target geometry without intermediate frames.
        session.shell.setMotionEnabled(false);
        session.theme.setMotionEnabled(false);
        layout->restoreShortcuts();
        QTest::qWait(50);
        home = item("sidebarShortcut-home");
        Test::moveNativePointer(session.window, center(home));
        QTRY_COMPARE(visual(home, "sidebarShortcutLabel")->opacity(), 1.);
        session.shell.toggleSidebar();
        QTRY_VERIFY(session.shell.sidebarCollapsed());
        QTest::qWait(50);
        home = item("sidebarShortcut-home");
        Test::moveNativePointer(session.window, center(home));
        QTRY_COMPARE(visual(home, "sidebarShortcutIcon")->opacity(), 1.);
        QVERIFY(!visual(home, "sidebarShortcutLabel")->isVisible());
        QVERIFY(!item("sidebarShortcutRemove-home")->isVisible());
        QVERIFY(capture("shortcut-collapsed-reduced-motion"));
        const auto railStart = center(home);
        const auto railEnd = center(item("sidebarShortcut-explore"));
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, railStart);
        QTest::mouseMove(session.window, railStart + QPoint(0, 10), 15);
        QTest::mouseMove(session.window, railEnd, 20);
        QTRY_COMPARE(layout->homeRoute(), QString("explore"));
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, railEnd);
        QCOMPARE(session.router.current()->name(), QString("profile"));

        for (const auto &arguments : warnings)
            for (const auto &warning : qvariant_cast<QList<QQmlError>>(arguments.value(0)))
                QVERIFY2(!warning.description().contains("SidebarShortcut"),
                         qPrintable(warning.toString()));
    }
    void sidebarHistoryNavigationRevealsPlaylistSection_data()
    {
        QTest::addColumn<bool>("collapsed");
        QTest::addColumn<bool>("darkScaled");
        QTest::newRow("expanded-light-100") << false << false;
        QTest::newRow("expanded-dark-125") << false << true;
        QTest::newRow("rail-light-100") << true << false;
        QTest::newRow("rail-dark-125") << true << true;
    }
    void sidebarHistoryNavigationRevealsPlaylistSection()
    {
        QFETCH(bool, collapsed);
        QFETCH(bool, darkScaled);
        Session session;
        session.shell.setMotionEnabled(false);
        if (collapsed)
            session.shell.toggleSidebar();
        if (darkScaled)
            while (session.shell.uiScale() < 1.25 - .001)
                session.shell.zoomIn();
        session.theme.setMode(darkScaled ? ThemeStore::Dark : ThemeStore::Light);
        PlaybackService playback(session.api, session.preferences, nullptr,
                                 session.directory.filePath("playback.sqlite"));
        SongMetadataService songMetadata(session.api);
        PlaylistDetailService details(session.api, playback, session.accounts.playlists,
                                      session.accounts.mutations, *session.accounts.library.order(),
                                      songMetadata, session.router, false);
        QVERIFY(session.create(&playback));
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        UserPlaylistModelTestAccess::present(session.accounts.playlists, session.api, 3);
        UserPlaylistModelTestAccess::collectSecond(session.accounts.playlists);
        connect(&session.accounts.library, &LibraryController::playlistRequested, &session.router,
                [&](const QString &id, quint64, bool) {
                    session.router.navigate("playlist-detail", {}, id);
                });
        const auto click = [&](const QString &name) {
            auto *item = visual(session.window->contentItem(), name);
            if (!item || !item->isVisible() || !item->isEnabled())
                return false;
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                              item->mapToScene({item->width() / 2, item->height() / 2}).toPoint());
            return true;
        };
        const auto selected = [&](const QString &id) {
            const auto matches = [&](const auto &self, QQuickItem *row) -> bool {
                if (row->objectName() == "sidebarPlaylistRow" && row->isVisible() &&
                    row->property("selected").toBool() &&
                    (row->property("kind").toString() == "owned"
                         ? row->property("globalId").toString()
                         : row->property("creatorGlobalId").toString()) == id)
                    return true;
                for (auto *child : row->childItems())
                    if (self(self, child))
                        return true;
                return false;
            };
            return matches(matches, session.window->contentItem());
        };
        QVERIFY(session.accounts.library.openPlaylist(1));
        QTRY_COMPARE(session.accounts.library.tab(), 0);
        QTRY_VERIFY(visual(session.window->contentItem(), "playlistDetailPage"));
        QTRY_VERIFY(selected("display-playlist-1"));
        QVERIFY(click(collapsed ? "sidebarRailPlaylistTab-1" : "sidebarCollectedTab"));
        QTRY_COMPARE(session.accounts.library.tab(), 1);
        QVERIFY(session.accounts.library.openPlaylist(2));
        QTRY_VERIFY(selected("history-collected-public"));
        QVERIFY(click("titlebarBackAction"));
        QTRY_COMPARE(session.router.current()->resourceId(), QString("display-playlist-1"));
        QTRY_COMPARE(session.accounts.library.tab(), 0);
        QTRY_VERIFY(selected("display-playlist-1"));
        QVERIFY(click("titlebarForwardAction"));
        QTRY_COMPARE(session.router.current()->resourceId(), QString("history-collected-public"));
        QTRY_COMPARE(session.accounts.library.tab(), 1);
        QTRY_VERIFY(selected("history-collected-public"));
        const auto captures = qEnvironmentVariable("ECHO_SIDEBAR_HISTORY_CAPTURE_DIR");
        if (!captures.isEmpty()) {
            QVERIFY(QDir().mkpath(captures));
            session.window->update();
            QTest::qWait(50);
            QVERIFY(session.window->grabWindow().save(
                captures + '/' + QString::fromLatin1(QTest::currentDataTag()) + ".png"));
        }
        session.router.shutdown();
        session.root.reset();
        session.window = nullptr;
        details.shutdown();
    }
    void productionSidebarScrollbar()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        UserPlaylistModelTestAccess::present(session.accounts.playlists, session.api, 1000);
        auto *scroll = visual(session.window->contentItem(), "sidebarPlaylistScroll");
        auto *bar = visual(scroll, "sidebarScrollBar");
        QVERIFY(scroll && bar);
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                for (const bool collapsed : {false, true}) {
                    if (session.shell.sidebarCollapsed() != collapsed)
                        session.shell.toggleSidebar();
                    QTRY_COMPARE(scroll->width(), collapsed ? 80. : 230.);
                    QTRY_VERIFY(bar->isVisible());
                    QVERIFY(bar->property("size").toReal() < 1);
                    QTRY_COMPARE(bar->x() + bar->width(), scroll->width());
                    QTest::mouseMove(session.window, scroll->mapToScene({40, 200}).toPoint());
                    QTRY_VERIFY(bar->property("revealed").toBool());
                    const auto captures =
                        qEnvironmentVariable("ECHO_SIDEBAR_SCROLLBAR_CAPTURE_DIR");
                    if (!captures.isEmpty()) {
                        QVERIFY(QDir().mkpath(captures));
                        QVERIFY(session.window->grabWindow().save(
                            captures + QString("/%1-%2-%3.png")
                                           .arg(mode == ThemeStore::Light ? "light" : "dark")
                                           .arg(zoomed ? "125" : "100")
                                           .arg(collapsed ? "rail" : "expanded")));
                    }
                    QTest::mouseMove(session.window, QPoint(700, 50));
                    QTRY_VERIFY(!bar->property("revealed").toBool());
                }
            }
        }
        session.window->hide();
        QTRY_VERIFY(!bar->property("resourceActive").toBool());
        session.window->show();
        QTRY_VERIFY(bar->property("resourceActive").toBool());
        UserPlaylistModelTestAccess::present(session.accounts.playlists, session.api, 0);
        session.window->resize(1150, 1200);
        QTRY_VERIFY(scroll->property("contentHeight").toReal() <= scroll->height());
        QTRY_VERIFY(!bar->isVisible());
        QCOMPARE(warnings.size(), 0);
    }
    void productionSidebarCardsMenusAndLayout()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        UserPlaylistModelTestAccess::present(session.accounts.playlists, session.api, 1000);
        session.theme.setMode(ThemeStore::Light);
        auto *layout = session.shell.sidebarLayout();
        const auto item = [&session](const QString &name) {
            return visual(session.window->contentItem(), name);
        };
        const auto center = [](QQuickItem *target) {
            return target->mapToScene(QPointF(target->width() / 2, target->height() / 2)).toPoint();
        };
        const auto capture = [&session](const QString &name) {
            const auto directory = qEnvironmentVariable("ECHO_SIDEBAR_COMPLETE_CAPTURE_DIR");
            if (directory.isEmpty())
                return true;
            QDir().mkpath(directory);
            session.window->update();
            QTest::qWait(100);
            return session.window->grabWindow().save(directory + "/" + name + ".png");
        };
        QTRY_VERIFY(item("sidebarPlaylistRow"));
        QVERIFY(visualCount(session.window->contentItem(), "sidebarPlaylistRow") < 30);
        QCOMPARE(session.accounts.library.playlists()->rowCount(), 1000);
        auto *scroll = item("sidebarPlaylistScroll");
        QVERIFY(scroll);
        QTRY_VERIFY(scroll->property("contentHeight").toReal() > scroll->height());
        QTest::qWait(100);
        const qreal before = scroll->property("contentY").toReal();
        const auto wheelPoint =
            scroll->mapToScene(QPointF(scroll->width() / 2, scroll->height() / 2));
        QTest::mouseMove(session.window, wheelPoint.toPoint());
        QWheelEvent wheel(wheelPoint, session.window->mapToGlobal(wheelPoint.toPoint()), QPoint(),
                          QPoint(0, -480), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(session.window, &wheel);
        QTRY_VERIFY(scroll->property("contentY").toReal() > before);
        QVERIFY(QMetaObject::invokeMethod(scroll, "cancelFlick"));
        // The inline header changes ListView's origin while its delegates
        // polish. Restore the beginning including that header, not stale contentY.
        QVERIFY(QMetaObject::invokeMethod(scroll, "positionViewAtBeginning"));
        QTRY_VERIFY(!scroll->property("moving").toBool());
        auto *sidebarBar = item("sidebarScrollBar");
        QVERIFY(sidebarBar);
        QTRY_VERIFY(sidebarBar->isVisible());
        QVERIFY(sidebarBar->property("size").toReal() < 1);
        QCOMPARE(sidebarBar->x() + sidebarBar->width(), scroll->width());
        QTRY_VERIFY(sidebarBar->property("revealed").toBool());
        QTest::mouseMove(session.window, QPoint(700, 50));
        QTRY_VERIFY(!sidebarBar->property("revealed").toBool());

        QCOMPARE(layout->sections()[1].toMap()["items"].toList().size(), 2);
        for (const auto &key : {"home", "explore", "free-listen", "personal-fm", "purchased"})
            QVERIFY(!item(QString("sidebarMenu-") + key));
        for (const auto &key : {"favorites", "cloud", "history"})
            QVERIFY(item(QString("sidebarMenu-") + key));
        auto *cards = item("sidebarShortcuts");
        auto *menu = item("sidebarMusicMenu");
        auto *header = item("sidebarPlaylistHeader");
        QVERIFY(cards && menu && header);
        QTRY_COMPARE(menu->mapToScene(QPointF()).y() -
                         cards->mapToScene(QPointF(0, cards->height())).y(),
                     24.0);
        QTRY_COMPARE(header->mapToScene(QPointF()).y() -
                         menu->mapToScene(QPointF(0, menu->height())).y(),
                     18.0);
        QCOMPARE(item("sidebarShortcutAdd")->height(), 28.0);
        const auto verifyPlaylistInset = [&item](bool collapsed) {
            auto *row = item("sidebarPlaylistRow");
            auto *cover = item("sidebarPlaylistCover");
            if (!row || !cover || !row->parentItem())
                return false;
            const auto width = row->parentItem()->width();
            const auto expectedX = collapsed ? (width - 42) / 2 : 18;
            if (!collapsed) {
                auto *tab = item("sidebarCreatedTab");
                auto *text = tab ? tab->property("contentItem").value<QQuickItem *>() : nullptr;
                if (!text ||
                    qAbs(cover->mapToScene(QPointF()).x() - text->mapToScene(QPointF()).x()) > .01)
                    return false;
            }
            return qAbs(row->x() - expectedX) < 0.01 &&
                   qAbs(cover->mapToItem(row, QPointF()).x() - (collapsed ? 5 : 14)) < 0.01 &&
                   qAbs(row->width() - (collapsed ? 42 : width - 34)) < 0.01;
        };
        QTRY_VERIFY(verifyPlaylistInset(false));
        QVERIFY(capture("sidebar-light"));
        session.theme.setMode(ThemeStore::Dark);
        QVERIFY(capture("sidebar-dark"));
        // Real pointer drag on the card body, without entering an editor.
        auto *home = item("sidebarShortcut-home");
        auto *explore = item("sidebarShortcut-explore");
        QVERIFY(home && explore);
        // Initial responsive sidebar width/Flow polish must finish before a
        // real pointer gesture. Captures used to incidentally supply this wait.
        QTRY_COMPARE(home->width(), 93.);
        QTRY_COMPARE(explore->x(), 101.);

        QPointer<QQuickItem> keptHome(home);
        QPointer<QQuickItem> keptExplore(explore);
        const auto headerStartY = cards->mapToScene(QPointF()).y();
        const auto start = center(home);
        const auto end =
            explore->mapToScene(QPointF(explore->width() * 0.8, explore->height() / 2)).toPoint();
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(session.window, start + QPoint(10, 0), 20);
        QTest::mouseMove(session.window, end, 40);
        auto *cardGhost = visual(cards, "home");
        QVERIFY(cardGhost);
        QVERIFY(cardGhost->isVisible());
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, end);
        QTRY_COMPARE(layout->shortcuts()[0].toMap()["key"].toString(), QString("explore"));
        QCOMPARE(session.router.current()->name(), QString("profile"));
        QVERIFY(keptHome && keptExplore);
        QCOMPARE(item("sidebarShortcut-home"), keptHome.data());
        QCOMPARE(item("sidebarShortcut-explore"), keptExplore.data());
        home = item("sidebarShortcut-home");
        QVERIFY(Test::prepareNativeInput(session.window));
        QTest::mouseMove(session.window, QPoint(700, 50), 20);
        QTest::mouseMove(session.window, center(home), 20);
        QTRY_VERIFY(item("sidebarShortcutRemove-home")->isVisible());
        QVERIFY(capture("sidebar-card-hover"));
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          center(item("sidebarShortcutRemove-home")));
        QTRY_COMPARE(layout->shortcuts().size(), 1);
        QTRY_COMPARE(item("sidebarShortcutAdd")->height(), 46.0);
        // Odd card counts used to allocate nearly all width to the add button.
        QTRY_COMPARE(item("sidebarShortcut-explore")->width(), item("sidebarShortcutAdd")->width());
        QCOMPARE(item("sidebarShortcut-explore")->width(), 93.0);
        QTest::mouseMove(session.window, center(item("sidebarShortcut-explore")), 20);
        QVERIFY(!item("sidebarShortcutRemove-explore")->isVisible());
        layout->toggleShortcut("explore");
        QCOMPARE(layout->shortcuts().size(), 1);
        layout->restoreShortcuts();
        QTRY_COMPARE(cards->mapToScene(QPointF()).y(), headerStartY);
        auto *popupOwner = session.root->findChild<QObject *>("sidebarLayoutPopupOwner");
        auto *popupTrigger = item("sidebarLayoutTrigger");
        QVERIFY(popupOwner && popupTrigger);
        const auto nativePopupCount = [popupOwner] {
            int count = 0;
            for (auto *child : popupOwner->findChildren<QObject *>())
                count += child->inherits("QQuickPopup");
            return count;
        };
        QVERIFY(!popupOwner->property("item").value<QObject *>());
        QVERIFY(!session.root->findChild<QObject *>("sidebarLayoutPopup"));
        QCOMPARE(nativePopupCount(), 0);
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, center(popupTrigger));
        QTRY_VERIFY(popupOwner->property("item").value<QObject *>());
        QPointer<QObject> popup = popupOwner->property("item").value<QObject *>();
        QTRY_VERIFY(popup->property("opened").toBool());
        QCOMPARE(nativePopupCount(), 1);
        QPointer<QObject> popupContent = popup->property("contentItem").value<QObject *>();
        QPointer<QObject> popupBackground = popup->property("background").value<QObject *>();
        QVERIFY(popupContent && popupBackground);
        auto *section = item("sidebarEditorSection-library");
        auto *groupEye = item("sidebarSectionVisibility-library");
        auto *rowEye = item("sidebarVisibility-favorites");
        QVERIFY(section && groupEye && rowEye);
        QCOMPARE(groupEye->mapToScene(QPointF(groupEye->width(), 0)).x(),
                 rowEye->mapToScene(QPointF(rowEye->width(), 0)).x());
        QCOMPARE(item("shellDragHandle-favorites")->width(), 16.0);
        QVERIFY(capture("sidebar-editor-dark"));
        auto *historyPointer = item("shellDragPointer-history");
        auto *first = item("sidebarEditorItem-favorites");
        QVERIFY(historyPointer && first);
        QCOMPARE(popup->property("side").toInt(), 1);
        const auto anchor = popup->property("anchorRect").toRectF();
        QCOMPARE(popup->property("x").toReal(), anchor.right() + 8);
        const auto dragStart = center(historyPointer);
        const auto dragEnd = first->mapToScene(QPointF(first->width() / 2, 5)).toPoint();
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, dragStart);
        QTest::mouseMove(session.window, dragStart + QPoint(0, -10), 20);
        QTest::mouseMove(session.window, dragEnd, 40);
        auto *dragVisual = historyPointer->parentItem()->findChild<QQuickItem *>("history");
        QVERIFY(dragVisual);
        QVERIFY(dragVisual->isVisible());
        QVERIFY(QQmlProperty(dragVisual, "Drag.active", qmlContext(dragVisual)).read().toBool());
        QTest::mouseMove(session.window, dragEnd + QPoint(1, 1), 20);
        QVERIFY(capture("sidebar-editor-dragging"));
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, dragEnd);
        QTRY_COMPARE(layout->data(layout->index(0), SidebarLayoutModel::Key).toString(),
                     QString("history"));
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          center(item("sidebarVisibility-playlist:created:default")));
        QTRY_COMPARE(session.accounts.library.playlists()->rowCount(), 999);
        QCOMPARE(session.accounts.playlists.rowCount(), 1000);
        QVERIFY(item("sidebarEditorItem-playlist:created:default"));
        QVERIFY(capture("sidebar-editor-hidden-default"));
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          center(item("sidebarSectionVisibility-playlists")));
        QTRY_VERIFY(!header->isVisible());
        QCOMPARE(session.accounts.playlists.rowCount(), 1000);
        layout->restoreDefaults();
        session.theme.setMode(ThemeStore::Light);
        QVERIFY(capture("sidebar-editor-light"));
        QTest::keyClick(session.window, Qt::Key_Escape);
        QTRY_VERIFY(popup.isNull());
        QTRY_VERIFY(popupContent.isNull());
        QTRY_VERIFY(popupBackground.isNull());
        QVERIFY(!popupOwner->property("item").value<QObject *>());
        QVERIFY(!session.root->findChild<QObject *>("sidebarLayoutPopup"));
        QCOMPARE(nativePopupCount(), 0);
        // Repeat the source-card gesture and popup anchoring in scaled coordinates.
        for (int i = 0; i < 5; ++i)
            session.shell.zoomIn();
        QCOMPARE(session.shell.uiScale(), 1.25);
        QTest::qWait(100);
        home = item("sidebarShortcut-home");
        explore = item("sidebarShortcut-explore");
        const auto scaledStart = center(home);
        const auto scaledEnd =
            explore->mapToScene(QPointF(explore->width() * 0.8, explore->height() / 2)).toPoint();
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, scaledStart);
        QTest::mouseMove(session.window, scaledStart + QPoint(12, 0), 20);
        QTest::mouseMove(session.window, scaledEnd, 40);
        // Native Cocoa updates Drag enter/leave during the next scene polish.
        QTest::qWait(40);
        QTest::mouseMove(session.window, scaledEnd + QPoint(1, 0), 20);
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, scaledEnd);
        QTRY_COMPARE(layout->shortcuts()[0].toMap()["key"].toString(), QString("explore"));
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, center(popupTrigger));
        QTRY_VERIFY(popupOwner->property("item").value<QObject *>());
        popup = popupOwner->property("item").value<QObject *>();
        QTRY_VERIFY(popup->property("opened").toBool());
        QCOMPARE(nativePopupCount(), 1);
        popupContent = popup->property("contentItem").value<QObject *>();
        popupBackground = popup->property("background").value<QObject *>();
        QVERIFY(popupContent && popupBackground);
        const auto scaledAnchor = popup->property("anchorRect").toRectF();
        QCOMPARE(popup->property("x").toReal(), scaledAnchor.right() + 8 * session.shell.uiScale());
        QTRY_VERIFY(verifyPlaylistInset(false));
        QVERIFY(capture("sidebar-editor-scaled-light"));
        QTest::keyClick(session.window, Qt::Key_Escape);
        QTRY_VERIFY(popup.isNull());
        QTRY_VERIFY(popupContent.isNull());
        QTRY_VERIFY(popupBackground.isNull());
        QVERIFY(!popupOwner->property("item").value<QObject *>());
        QVERIFY(!session.root->findChild<QObject *>("sidebarLayoutPopup"));
        QCOMPARE(nativePopupCount(), 0);
        session.shell.resetZoom();
        session.shell.toggleSidebar();
        QTRY_COMPARE(item("sidebarPlaylistHeader")->height(), 30.0);
        QTRY_VERIFY(sidebarBar->isVisible());
        QTRY_COMPARE(sidebarBar->x() + sidebarBar->width(), scroll->width());
        QTRY_COMPARE(item("sidebarShortcutAdd")->height(), 40.0);
        QTRY_COMPARE(item("sidebarShortcutAdd")->width(), 44.0);
        QCOMPARE(item("sidebarShortcut-home")->width(), 44.0);
        QCOMPARE(item("sidebarShortcuts")->mapToScene(QPointF()).x(), 18.0);
        auto *railMenu = item("sidebarMenu-favorites");
        QVERIFY(railMenu);
        QTRY_COMPARE(railMenu->width(), 38.0);
        QCOMPARE(railMenu->height(), 38.0);
        QCOMPARE(railMenu->mapToScene(QPointF()).x(), 21.0);
        QTRY_VERIFY(verifyPlaylistInset(true));
        auto *avatarButton = item("sidebarUserLink");
        auto *avatar = item("sidebarAccountAvatar");
        auto *avatarBackground = item("sidebarUserLinkBackground");
        QVERIFY(avatarButton && avatar && avatarBackground);
        QTRY_COMPARE(avatarButton->width(), 42.0);
        QCOMPARE(avatarButton->height(), 42.0);
        QCOMPARE(avatar->width(), 32.0);
        QCOMPARE(avatar->height(), 32.0);
        QCOMPARE(avatar->mapToItem(avatarButton, QPointF()), QPointF(5, 5));
        QCOMPARE(avatarBackground->property("radius").toReal(), 10.0);
        QVERIFY(!item("sidebarAccountTrigger")->isVisible());
        QTest::mouseMove(session.window, QPoint(700, 50), 20);
        QCOMPARE(avatarBackground->property("color").value<QColor>(), session.theme.mutedSurface());
        QVERIFY(capture("sidebar-rail-light"));
        QTest::mouseMove(session.window, center(avatarButton), 20);
        QTRY_VERIFY(avatarButton->property("hovered").toBool());
        QCOMPARE(avatarBackground->property("color").value<QColor>(), session.theme.controlHover());
        session.theme.setMode(ThemeStore::Dark);
        QTest::mouseMove(session.window, QPoint(700, 50), 20);
        QVERIFY(capture("sidebar-rail-dark"));
        session.shell.zoomIn();
        QTRY_VERIFY(verifyPlaylistInset(true));
        QCOMPARE(avatar->mapToItem(avatarButton, QPointF()), QPointF(5, 5));
        QVERIFY(capture("sidebar-rail-scaled-dark"));
        UserPlaylistModelTestAccess::present(session.accounts.playlists, session.api, 0);
        session.window->resize(1150, 1200);
        QTRY_VERIFY(scroll->property("contentHeight").toReal() <= scroll->height());
        QTRY_VERIFY(!sidebarBar->isVisible());
        QCOMPARE(warnings.size(), 0);
    }
    void signedOutAccountPopoverAndOddShortcutWidths()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *layout = session.shell.sidebarLayout();
        auto *panel = session.root->findChild<QObject *>("sidebarAccountPopover");
        QVERIFY(panel);
        const auto item = [&session](const QString &name) {
            return visual(session.window->contentItem(), name);
        };
        const auto capture = qEnvironmentVariable("ECHO_SIDEBAR_COMPLETE_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        // All three grid cases use the production incremental shortcut model.
        for (const int count : {2, 1, 3}) {
            layout->restoreShortcuts();
            if (count == 1)
                layout->toggleShortcut("home");
            else if (count == 3)
                layout->toggleShortcut("free-listen");
            QTRY_COMPARE(layout->shortcuts().size(), count);
            for (const auto &entry : layout->shortcuts()) {
                auto *card = item("sidebarShortcut-" + entry.toMap()["key"].toString());
                QVERIFY(card);
                QTRY_COMPARE(card->width(), 93.0);
            }
            if (count % 2)
                QTRY_COMPARE(item("sidebarShortcutAdd")->width(), 93.0);
        }
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    session.shell.zoomIn();
                for (const bool saved : {false, true}) {
                    StoredAccounts rows;
                    if (saved)
                        rows.append({900001, "保存账号", {}, {}, 1});
                    SavedAccountModelTestAccess::present(session.accounts.saved, rows, {});
                    auto *trigger = item("sidebarAccountTrigger");
                    QVERIFY(trigger);
                    QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                                      trigger->mapToScene({14, 22}).toPoint());
                    QTRY_VERIFY(panel->property("opened").toBool());
                    auto *login = item("accountLoginAction");
                    auto *switchAction = item("accountSignedOutSwitchAction");
                    QVERIFY(login && switchAction);
                    QCOMPARE(login->height(), 32.0);
                    QCOMPARE(switchAction->height(), 32.0);
                    QCOMPARE(switchAction->isVisible(), saved);
                    auto *content = panel->property("contentItem").value<QQuickItem *>();
                    QVERIFY(content);
                    QVERIFY(content->width() >= login->width());
                    if (!capture.isEmpty()) {
                        session.window->update();
                        QTest::qWait(100);
                        QVERIFY(session.window->grabWindow().save(
                            capture + QString("/signed-out-%1-%2-%3.png")
                                          .arg(mode == ThemeStore::Light ? "light" : "dark")
                                          .arg(zoomed ? "scaled" : "normal")
                                          .arg(saved ? "saved" : "empty")));
                    }
                    QVERIFY(QMetaObject::invokeMethod(panel, "close"));
                    QTRY_VERIFY(!panel->property("visible").toBool());
                }
            }
        }
        session.shell.resetZoom();
        auto *trigger = item("sidebarAccountTrigger");
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          trigger->mapToScene({14, 22}).toPoint());
        QTRY_VERIFY(panel->property("opened").toBool());
        auto *login = item("accountLoginAction");
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          login->mapToScene({login->width() / 2, 16}).toPoint());
        QTRY_VERIFY(session.accounts.controller.loginOpen());
        QTRY_VERIFY(!panel->property("visible").toBool());
        session.accounts.controller.closeLogin();
        QCOMPARE(warnings.size(), 0);
    }
    void remainingSettingsProductionSections()
    {
        Session session;
        session.shell.setMotionEnabled(false);
        LogService logging(session.directory.filePath("logs"), 2, false, &session.preferences);
        PlaybackService playback(session.api, session.preferences, nullptr,
                                 session.directory.filePath("playback.sqlite"));
        NetworkPreferences network(session.preferences, session.api);
        ShortcutService shortcuts(session.preferences, false);
        AboutService about(session.preferences, network);
        RelaunchService relaunch({}, {}, false);
        PluginRuntime plugins(session.engine, session.preferences,
                              {playback, session.theme, session.commands, session.router,
                               session.accounts.account, session.api},
                              session.directory.filePath("plugins"), true);
        SettingsDataService data(session.preferences, plugins, relaunch, network,
                                 session.directory.path(), about.version());
        QVERIFY(session.create(&playback, &network, &logging, &relaunch));
        session.root->setProperty("shortcutService", QVariant::fromValue(&shortcuts));
        session.root->setProperty("aboutService", QVariant::fromValue(&about));
        session.root->setProperty("settingsDataService", QVariant::fromValue(&data));
        shortcuts.attach(session.window);
        // Native Popup Escape handling requires an active platform window;
        // rendering alone also works while the test process is in background.
        session.window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(session.window));
        QStringList errors;
        connect(&session.engine, &QQmlEngine::warnings, this,
                [&](const QList<QQmlError> &warnings) {
                    for (const auto &warning : warnings)
                        if (warning.url().toString().contains("/Settings/"))
                            errors.append(warning.toString());
                });
        session.router.navigate("settings");
        const auto captures = qEnvironmentVariable("ECHO_SETTINGS_PAGES_ARTIFACTS");
        if (!captures.isEmpty())
            QVERIFY(QDir().mkpath(captures));
        for (bool dark : {false, true}) {
            session.theme.setMode(dark ? ThemeStore::Dark : ThemeStore::Light);
            for (int zoom : {100, 125}) {
                // 每次关闭会销毁完整弹层。重开后重新获取弹层、catalog 和 section loader，
                // 不要跨迭代保留这些对象的裸指针。
                QPointer<QObject> dialog = session.root->findChild<QObject *>("settingsDialog");
                QVERIFY(dialog);
                QTRY_VERIFY(dialog->property("opened").toBool());
                auto *catalog = dialog->property("catalog").value<SettingsCatalog *>();
                QVERIFY(catalog);
                auto *loader = visual(session.window->contentItem(), "settingsSectionLoader");
                QVERIFY(loader);
                session.shell.resetZoom();
                if (zoom == 125)
                    for (int i = 0; i < 5; ++i)
                        session.shell.zoomIn();
                int actionCount = 0;
                for (const auto *key : {"interface", "window", "font", "playback", "player",
                                        "spatialAudio", "lyric", "shortcuts", "audioDevice", "cast",
                                        "plugins", "data", "network", "experimental", "about"}) {
                    catalog->select(key);
                    QTRY_COMPARE(loader->property("source").toUrl(), catalog->source());
                    QTRY_COMPARE(loader->property("status").toInt(), 1); // Loader.Ready
                    auto *section = loader->property("item").value<QQuickItem *>();
                    QVERIFY(section);
                    QTest::qWait(50);
                    // Walk real instantiated controls, including content below the
                    // scroll viewport. Check allocated geometry after layout so a
                    // row override/shrink cannot pass on implicit size alone.
                    QList<QQuickItem *> pending{section};
                    while (!pending.isEmpty()) {
                        auto *item = pending.takeLast();
                        pending.append(item->childItems());
                        if (!item->isVisible())
                            continue;
                        bool action = false;
                        for (auto *meta = item->metaObject(); meta; meta = meta->superClass()) {
                            const QByteArray name(meta->className());
                            action |= name == "EchoSoftButton" ||
                                      name.startsWith("EchoSoftButton_") || name == "EchoButton" ||
                                      name.startsWith("EchoButton_");
                        }
                        if (!action)
                            continue;
                        ++actionCount;
                        const auto context = QString("%1 / %2: %3px")
                                                 .arg(key, item->property("text").toString())
                                                 .arg(item->height());
                        QVERIFY2(qAbs(item->height() - 36) < .01, qPrintable(context));
                        const auto sceneHeight =
                            item->mapRectToScene(item->boundingRect()).height();
                        QVERIFY2(qAbs(sceneHeight - 36 * session.shell.uiScale()) < .05,
                                 qPrintable(context));
                    }
                    if (!captures.isEmpty()) {
                        const auto pixels = session.window->grabWindow();
                        QVERIFY(!pixels.isNull());
                        QVERIFY(pixels.save(captures + QString("/%1-%2-%3.png")
                                                           .arg(key)
                                                           .arg(dark ? "dark" : "light")
                                                           .arg(zoom)));
                    }
                }
                QVERIFY(actionCount >= 25);
                auto *changelog = visual(session.window->contentItem(), "settingsChangelog");
                QVERIFY(changelog);
                QMetaObject::invokeMethod(changelog, "clicked");
                QPointer<QObject> changelogDialog =
                    session.root->findChild<QObject *>("settingsChangelogDialog");
                QVERIFY(changelogDialog);
                QTRY_VERIFY(changelogDialog->property("opened").toBool());
                QVERIFY(visual(session.window->contentItem(), "settingsChangelogText"));
                if (!captures.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        captures +
                        QString("/changelog-%1-%2.png").arg(dark ? "dark" : "light").arg(zoom)));
                QTest::keyClick(session.window, Qt::Key_Escape);
                QTRY_VERIFY(!changelogDialog || !changelogDialog->property("visible").toBool());
                QVERIFY2(dialog->property("opened").toBool(),
                         "Closing changelog must preserve settings");
                auto *declaration = visual(session.window->contentItem(), "settingsDisclaimer");
                QVERIFY(declaration);
                QMetaObject::invokeMethod(declaration, "clicked");
                QObject *disclaimerDialog = nullptr;
                QTRY_VERIFY((disclaimerDialog =
                                 session.root->findChild<QObject *>("settingsDisclaimerDialog")) &&
                            disclaimerDialog->property("opened").toBool());
                if (!captures.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        captures +
                        QString("/disclaimer-%1-%2.png").arg(dark ? "dark" : "light").arg(zoom)));
                QTest::keyClick(session.window, Qt::Key_Escape);
                QTRY_VERIFY([&] {
                    const auto *disclaimer =
                        session.root->findChild<QObject *>("settingsDisclaimerDialog");
                    return disclaimer == nullptr || !disclaimer->property("visible").toBool();
                }());
                QVERIFY2(dialog->property("opened").toBool(),
                         "Closing disclaimer must preserve settings");
                catalog->select("data");
                QTRY_VERIFY(visual(session.window->contentItem(), "settingsCreateBackup"));
                QMetaObject::invokeMethod(
                    visual(session.window->contentItem(), "settingsCreateBackup"), "clicked");
                QObject *backupDialog = nullptr;
                QTRY_VERIFY((backupDialog = session.root->findChild<QObject *>(
                                 "settingsBackupExportDialog")) &&
                            backupDialog->property("opened").toBool());
                if (!captures.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        captures +
                        QString("/backup-%1-%2.png").arg(dark ? "dark" : "light").arg(zoom)));
                QTest::keyClick(session.window, Qt::Key_Escape);
                QMetaObject::invokeMethod(dialog, "close");
                QTRY_VERIFY(!dialog || !dialog->property("visible").toBool());
                QVERIFY(session.commands.invokePlayerCommand(ShellController::Cast));
                QObject *castPopup = nullptr;
                QTRY_VERIFY((castPopup = session.root->findChild<QObject *>("playbackCastPopup")) &&
                            castPopup->property("opened").toBool());
                QVERIFY(playback.cast()->browsing());
                if (!captures.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        captures +
                        QString("/cast-popup-%1-%2.png").arg(dark ? "dark" : "light").arg(zoom)));
                auto *castSettings = visual(session.window->contentItem(), "playbackCastSettings");
                QVERIFY(castSettings);
                QMetaObject::invokeMethod(castSettings, "clicked");
                // 弹层按需创建：settingsDialog 关闭后已释放，重新获取实例。
                QObject *reopened = nullptr;
                QTRY_VERIFY((reopened = session.root->findChild<QObject *>("settingsDialog")) &&
                            reopened->property("opened").toBool());
                auto *reopenedCatalog = reopened->property("catalog").value<SettingsCatalog *>();
                QVERIFY(reopenedCatalog);
                QTRY_COMPARE(reopenedCatalog->current(), QString("cast"));
                QTRY_VERIFY(visual(session.window->contentItem(), "settingsCastEnabled"));
            }
        }
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join('\n')));
        about.shutdown();
        data.shutdown();
        shortcuts.shutdown();
        session.root.reset();
        session.window = nullptr;
        QSignalSpy pluginClosed(&plugins, &PluginRuntime::closed);
        plugins.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(pluginClosed.size(), 1, 15000);
        QSignalSpy playbackClosed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(playbackClosed.size(), 1, 15000);
    }
    void settingsFilterAndUnsupportedProviderNeverLoadStub()
    {
        SettingsCatalog catalog;
        QAbstractItemModelTester tester(&catalog,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(catalog.rowCount(), 15);
        catalog.setFilter(" 字 体 ");
        QVERIFY(catalog.rowCount() > 0);
        QCOMPARE(catalog.current(), QString("font"));
        catalog.setFilter("音效");
        QCOMPARE(catalog.rowCount(), 0);
        QVERIFY(catalog.source().isEmpty());
        catalog.select("spatialAudio");
        QVERIFY(catalog.source().isEmpty());
        // Check exact setting labels and aliases; generic proxy/log matches
        // do not cover the full settings search.
        for (const auto &keyword : {"proxyRules", "bypass", "代理密码", "播放器网络超时"}) {
            catalog.setFilter(QString::fromUtf8(keyword));
            QCOMPARE(catalog.rowCount(), 1);
            QCOMPARE(catalog.current(), QString("network"));
            QVERIFY(!catalog.source().isEmpty());
        }
        catalog.setFilter("在线插件源");
        QCOMPARE(catalog.rowCount(), 2); // Source lists this in plugins and network.
        QCOMPARE(catalog.current(), QString("plugins"));
        catalog.select("network");
        QCOMPARE(catalog.current(), QString("network"));
        for (const auto &keyword : {"API 响应体日志", "临时诊断日志", "诊断并重启"}) {
            catalog.setFilter(QString::fromUtf8(keyword));
            QCOMPARE(catalog.rowCount(), 1);
            QCOMPARE(catalog.current(), QString("experimental"));
        }
        catalog.setFilter("全屏按钮");
#ifdef Q_OS_MACOS
        QCOMPARE(catalog.rowCount(), 0);
#else
        QCOMPARE(catalog.current(), QString("interface"));
#endif
        catalog.setFilter("not-a-real-setting");
        QCOMPARE(catalog.rowCount(), 0);
        catalog.reset();
        QCOMPARE(catalog.current(), QString("interface"));
    }
    void accountProfileAndGradeRejectOldAccountResult()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService service(api);
        QVERIFY(AccountServiceTestAccess::adopt(service, account(900001)));
        const auto detail = AccountServiceTestAccess::profileRequest(service, api);
        AccountServiceTestAccess::complete(
            service, detail,
            R"({"id":900001,"nickname":"Qt 测试账号","avatar_url":"","biography":"说明","grade":5,"gender":1,"province":"上海","city":"上海","visitors":42,"registered_at_seconds":1700000000,"listening_minutes":100,"friends":2,"follows":3,"fans":4})");
        QCOMPARE(service.genderLabel(), QStringLiteral("男"));
        QCOMPARE(service.location(), QStringLiteral("上海"));
        QCOMPARE(service.visitors(), quint64(42));
        QVERIFY(service.accountAge() != QStringLiteral("未知"));
        const auto grade = AccountServiceTestAccess::gradeRequest(service, api);
        AccountServiceTestAccess::complete(
            service, grade,
            R"({"user_id":900001,"grade":5,"listening_seconds":864577,"current_points":17120,"next_grade_points":20000,"next_grade":6})");
        QVERIFY(service.gradeAvailable());
        QCOMPARE(service.nextGrade(), 6);
        QCOMPARE(service.listeningDuration(), QStringLiteral("10 天 9 分钟"));
        const auto stale = AccountServiceTestAccess::profileRequest(service, api);
        QVERIFY(AccountServiceTestAccess::adopt(service, account(900002)));
        AccountServiceTestAccess::complete(service, stale,
                                           R"({"id":900001,"visitors":999,"province":"旧账号"})");
        QCOMPARE(service.visitors(), quint64(0));
        QCOMPARE(service.location(), QStringLiteral("未知"));
        QVERIFY(!service.gradeAvailable());
        QVERIFY(service.studentSchool().isEmpty());
        api.shutdown();
    }
    void identityProjectionAndSessionIsolation()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService service(api);
        QVERIFY(AccountServiceTestAccess::adopt(service, account(900001)));
        const auto detail = AccountServiceTestAccess::profileRequest(service, api);
        AccountServiceTestAccess::complete(
            service, detail,
            R"({"id":900001,"identity":{"talent_bits":3,"actor":true,"business":true,"star":true,"singer":true,"authorizations":[],"talent_authorizations":[],"singer_authorizations":[]}})");
        const auto identity = AccountServiceTestAccess::identityRequest(service, api);
        AccountServiceTestAccess::complete(
            service, identity,
            R"({"student_status":1,"student_school":"领域测试学校","student_expire_time":"2030-01-01","tags":["摇滚","旅行"]})");
        QCOMPARE(service.avatarIdentityKind(), QString("singer"));
        QCOMPARE(service.identityBadges().size(), 7);
        QCOMPARE(service.identityBadges().first().toMap()["label"].toString(),
                 QStringLiteral("学生"));
        QCOMPARE(service.identityBadges().last().toMap()["label"].toString(),
                 QStringLiteral("评论达人"));
        QCOMPARE(service.identityTags(),
                 (QStringList{QStringLiteral("摇滚"), QStringLiteral("旅行")}));
        const auto stale = AccountServiceTestAccess::identityRequest(service, api);
        QVERIFY(AccountServiceTestAccess::adopt(service, account(900002)));
        AccountServiceTestAccess::complete(service, stale,
                                           R"({"student_status":1,"tags":["旧账号"]})");
        QVERIFY(service.identityBadges().isEmpty());
        QVERIFY(service.identityTags().isEmpty());
        QVERIFY(service.avatarIdentityIcon().isEmpty());
        QVERIFY(service.studentSchool().isEmpty());
        // Product projection tests use canonical data, not invented HTTP fixtures.
        AccountIdentity talent;
        talent.talentBits = 1 | (1 << 11);
        talent.authorizations = {QStringLiteral("歌单达人")};
        talent.talentAuthorizations = {QStringLiteral("歌单达人")};
        QCOMPARE(talent.avatarKind(), QString("talent"));
        QCOMPARE(talent.badges(false).size(), 1);
        talent.authorizations.clear();
        talent.talentAuthorizations.clear();
        QCOMPARE(talent.badges(false).size(), 1); // Reserved category has no label.
        api.shutdown();
    }
    void productionIdentitySummaryAndAvatarCorners()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        // The page exists before network data arrives in the real application.
        // Exercise empty-to-populated summaries, rather than preloading every tag.
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profileIdentitySummary"));
        auto *summary = visual(session.window->contentItem(), "profileIdentitySummary");
        QTest::qWait(50);
        const auto detail =
            AccountServiceTestAccess::profileRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, detail,
            R"({"id":900001,"nickname":"一天 Qt","avatar_url":"","identity":{"talent_bits":3,"actor":false,"business":true,"star":false,"singer":false,"authorizations":[],"talent_authorizations":[],"singer_authorizations":[]}})");
        const auto identity =
            AccountServiceTestAccess::identityRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, identity,
            R"({"student_status":1,"tags":["摇滚","English 中文","用于验证长标签换行和主题颜色不会挤出个人中心边界的领域展示文字"]})");
        QCOMPARE(visual(session.window->contentItem(), "profileIdentitySummary"), summary);
        QTRY_VERIFY(summary->width() > 0);
        QTRY_COMPARE(visualAll(summary, "userIdentityBadge").size(), 4);
        QTRY_COMPARE(visualAll(summary, "userInterestTag").size(), 3);
        // Repeater.count changes before itemAt() is populated on Cocoa.
        // Verify the original page grows as delegates arrive, before a resize,
        // theme change or zoom could incidentally recompute its natural width.
        QTRY_VERIFY(summary->height() >= 18);
        QTRY_VERIFY(summary->property("naturalWidth").toReal() > 10);
        for (auto *tag : visualAll(summary, "userInterestTag"))
            QTRY_VERIFY(tag->width() > 0);
        auto *profileAvatar = visual(session.window->contentItem(), "profileAccountAvatar");
        QVERIFY(profileAvatar);
        QCOMPARE(profileAvatar->property("identityInset").toReal(), 3.0);
        QCOMPARE(profileAvatar->property("identityIcon").toUrl(),
                 QUrl(session.accounts.account.avatarIdentityIcon()));
        auto *avatar = visual(session.window->contentItem(), "sidebarAccountAvatar");
        QVERIFY(avatar);
        QCOMPARE(avatar->property("identityIcon").toUrl(),
                 QUrl(session.accounts.account.avatarIdentityIcon()));
        // Local icon input exercises the real mark loader without making a
        // network image download a prerequisite of a geometry regression.
        QVERIFY(avatar->setProperty("identityIcon",
                                    QUrl("qrc:/qt/qml/EchoMusic/Controls/icons/user.svg")));
        auto *mark = avatar->findChild<QQuickItem *>("avatarIdentityMark");
        QVERIFY(mark);
        QTRY_VERIFY(mark->isVisible());
        QCOMPARE(mark->width(), 16.0);
        QCOMPARE(mark->height(), 16.0);
        QCOMPARE(mark->x(), avatar->width() - 17);
        QCOMPARE(mark->y(), avatar->height() - 17);
        avatar->setProperty("resourceActive", false);
        QTRY_VERIFY(mark->property("source").toUrl().isEmpty());
        avatar->setProperty("resourceActive", true);
        QTRY_VERIFY(mark->isVisible());
        const auto capture = qEnvironmentVariable("ECHO_IDENTITY_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                QTRY_VERIFY(summary->height() >= 18);
                for (auto *tag : visualAll(summary, "userInterestTag")) {
                    QTRY_VERIFY(tag->width() > 0);
                    const auto position = tag->mapToItem(summary, QPointF());
                    QVERIFY(position.x() >= -0.5);
                    QVERIFY(position.x() + tag->width() <= summary->width() + 0.5);
                    QVERIFY(position.y() + tag->height() <= summary->height() + 0.5);
                }
                if (!capture.isEmpty()) {
                    QTest::qWait(120);
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/identity-%1-%2.png")
                                      .arg(mode == ThemeStore::Light ? "light" : "dark")
                                      .arg(zoomed ? "scaled" : "normal")));
                }
            }
        }
        QQmlComponent narrowComponent(&session.engine,
                                      QUrl("qrc:/qt/qml/EchoMusic/Profile/ProfileOverview.qml"));
        std::unique_ptr<QObject> narrow(narrowComponent.createWithInitialProperties(
            {{"account", QVariant::fromValue(&session.accounts.account)}, {"width", 360.0}}));
        QVERIFY2(narrow, qPrintable(narrowComponent.errorString()));
        auto *narrowItem = qobject_cast<QQuickItem *>(narrow.get());
        QVERIFY(narrowItem);
        narrowItem->setParentItem(session.window->contentItem());
        auto *narrowSummary = visual(narrowItem, "profileIdentitySummary");
        auto *narrowAvatar = visual(narrowItem, "profileAccountAvatar");
        QVERIFY(narrowSummary && narrowAvatar);
        QTRY_VERIFY(narrowSummary->height() > 50);
        QVERIFY(narrowSummary->mapToItem(narrowItem, QPointF()).y() <
                narrowAvatar->mapToItem(narrowItem, QPointF()).y());
        for (auto *tag : visualAll(narrowSummary, "userInterestTag")) {
            const auto position = tag->mapToItem(narrowSummary, QPointF());
            QVERIFY(position.x() >= -0.5);
            QVERIFY(position.x() + tag->width() <= narrowSummary->width() + 0.5);
            QVERIFY(position.y() + tag->height() <= narrowSummary->height() + 0.5);
        }
        narrow.reset();
        session.shell.toggleSidebar();
        QTRY_COMPARE(avatar->width(), 32.0);
        QCOMPARE(mark->x(), 15.0);
        session.shell.toggleSidebar();
        auto *panel = session.root->findChild<QObject *>("sidebarAccountPopover");
        QVERIFY(panel);
        QVERIFY(QMetaObject::invokeMethod(panel, "open"));
        QTRY_VERIFY(panel->property("opened").toBool());
        auto *popupAvatar = visual(session.window->contentItem(), "accountPopoverAvatar");
        QVERIFY(popupAvatar);
        QCOMPARE(popupAvatar->property("identityIcon").toUrl(),
                 QUrl(session.accounts.account.avatarIdentityIcon()));
        QVERIFY(QMetaObject::invokeMethod(panel, "close"));
        QTRY_VERIFY(!panel->property("visible").toBool());
        QTRY_VERIFY(popupAvatar->findChild<QQuickItem *>("avatarIdentityMark")
                        ->property("source")
                        .toUrl()
                        .isEmpty());
        StoredAccounts rows{{900001, "Current", {}, {}, 2},
                            {900002, "Saved singer", {}, {}, 1, "singer"}};
        SavedAccountModelTestAccess::present(
            session.accounts.saved, rows,
            AccountServiceTestAccess::snapshot(session.accounts.account));
        auto *switcher = session.root->findChild<QObject *>("accountSwitcherDialog");
        QVERIFY(switcher);
        QVERIFY(QMetaObject::invokeMethod(switcher, "open"));
        QTRY_VERIFY(switcher->property("opened").toBool());
        auto *list = visual(session.window->contentItem(), "savedAccountList");
        QVERIFY(list);
        QTRY_COMPARE(visualAll(list, "savedAccountAvatar").size(), 2);
        bool singerFound = false;
        for (auto *savedAvatar : visualAll(list, "savedAccountAvatar")) {
            QCOMPARE(savedAvatar->property("identityInset").toReal(), 0.0);
            singerFound |=
                savedAvatar->property("identityLabel").toString() == QStringLiteral("歌手认证");
        }
        QVERIFY(singerFound);
        QVERIFY(QMetaObject::invokeMethod(switcher, "close"));
        QCOMPARE(warnings.size(), 0);
    }
    void globalFontUsesInstalledFamilyAndRestoresPreference()
    {
        QTemporaryDir directory;
        QSettings preferences(directory.filePath("font.ini"), QSettings::IniFormat);
        ThemeStore theme(preferences);
        const auto family = QFontDatabase::families().value(0);
        QVERIFY(!family.isEmpty());
        QSignalSpy changed(&theme, &ThemeStore::fontChanged);
        theme.setGlobalFont(family);
        QCOMPARE(theme.globalFont(), family);
        QCOMPARE(theme.applicationFont().families().first(), family);
        QTRY_VERIFY(!changed.isEmpty());
        theme.setGlobalFont("no-such-installed-family");
        QCOMPARE(theme.globalFont(), family);
        preferences.sync();
        QSettings reopened(directory.filePath("font.ini"), QSettings::IniFormat);
        ThemeStore restored(reopened);
        QCOMPARE(restored.globalFont(), family);
        restored.setGlobalFont("system-ui");
    }
    void fixedProfileSocialAndSidebarActionsKeepIntrinsicWidths()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profileSwitchAccount"));
        auto *first = visual(session.window->contentItem(), "profileSwitchAccount");
        auto *last = visual(session.window->contentItem(), "profileLogout");
        auto *actions = first->parentItem();
        auto *heading = actions->parentItem();
        auto *order = visual(session.window->contentItem(), "sidebarPlaylistOrder");
        QVERIFY(last && order);
        auto *sidebarActions = order->parentItem();
        QQmlComponent component(&session.engine,
                                QUrl("qrc:/qt/qml/EchoMusic/Profile/SocialUserRow.qml"));
        // Canonical display inputs, never a fabricated upstream response sample.
        std::unique_ptr<QObject> social(component.createWithInitialProperties(
            {{"entryKey", "user:77"},
             {"userId", 77},
             {"nickname", QStringLiteral("很长的中文昵称 Long Latin nickname")},
             {"avatarUrl", ""},
             {"avatarIdentityIcon", QUrl()},
             {"avatarIdentityLabel", ""},
             {"identityBadges", QVariantList()},
             {"description", QStringLiteral("中文 Latin description")},
             {"meta", "Fans 100"},
             {"following", false},
             {"canUnfollow", false},
             {"relationshipKnown", true}}));
        QVERIFY2(social, qPrintable(component.errorString()));
        auto *socialItem = qobject_cast<QQuickItem *>(social.get());
        QVERIFY(socialItem);
        socialItem->setParentItem(session.window->contentItem());
        socialItem->setPosition({300, 460});
        QQuickItem *socialActions = nullptr;
        for (auto *child : socialItem->childItems().first()->childItems().first()->childItems())
            if (child->childItems().size() == 2 &&
                child->childItems().first()->property("text").toString() == QStringLiteral("关注"))
                socialActions = child;
        QVERIFY(socialActions);
        const auto capture = qEnvironmentVariable("ECHO_LAYOUT_ALLOCATION_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        session.theme.setMotionEnabled(false);
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const auto scale : {1.0, 1.25}) {
                session.shell.resetZoom();
                if (scale > 1)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                socialItem->setScale(scale);
                for (const int width : {1150, 1450}) {
                    session.window->resize(width, 850);
                    QTRY_COMPARE(actions->width(), 212.);
                    QTRY_COMPARE(
                        visualAll(session.window->contentItem(), "profileStatisticValue").size(),
                        5);
                    const auto values =
                        visualAll(session.window->contentItem(), "profileStatisticValue");
                    QCOMPARE(values.size(), 5);
                    for (auto *value : values) {
                        auto *row = value->parentItem();
                        auto *stat = row->parentItem();
                        // Native layouts distribute the fractional remainder across physical
                        // columns.
                        QTRY_VERIFY2(qAbs(stat->width() - stat->parentItem()->width() / 5) <= 1.,
                                     qPrintable(QString("stat=%1 grid=%2 parent=%3")
                                                    .arg(stat->width())
                                                    .arg(stat->parentItem()->width())
                                                    .arg(stat->metaObject()->className())));
                        QTRY_VERIFY(visual(row, "profileStatisticLabel"));
                        auto *caption = visual(row, "profileStatisticLabel");
                        QVERIFY(caption);
                        QTRY_COMPARE(value->width(), value->implicitWidth());
                        QTRY_VERIFY(qAbs((value->x() + caption->x() + caption->width()) / 2 -
                                         row->width() / 2) < .6);
                    }
                    QTRY_COMPARE(last->mapToItem(heading, {36, 0}).x(), heading->width());
                    QTRY_COMPARE(last->mapToItem(actions, {0, 0}).x(), 176.);
                    QTRY_COMPARE(sidebarActions->width(), 76.);
                    QTRY_COMPARE(
                        sidebarActions->mapToItem(sidebarActions->parentItem(), {76, 0}).x(),
                        sidebarActions->parentItem()->width());
                    for (const bool known : {false, true}) {
                        social->setProperty("relationshipKnown", known);
                        for (const int rowWidth : {360, 540}) {
                            socialItem->setWidth(rowWidth);
                            QTRY_COMPARE(socialActions->width(), socialActions->implicitWidth());
                            QTRY_VERIFY(qAbs(socialActions->x() + socialActions->width() -
                                             socialActions->parentItem()->width()) < .5);
                        }
                    }
                    if (!capture.isEmpty()) {
                        session.window->update();
                        QSignalSpy frame(session.window, &QQuickWindow::frameSwapped);
                        QVERIFY(frame.wait());
                        QVERIFY(session.window->grabWindow().save(
                            capture + QString("/fixed-actions-%1-%2-%3.png")
                                          .arg(int(mode))
                                          .arg(scale)
                                          .arg(width)));
                    }
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void productionProfileNameTagsStayAligned()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        // Canonical display data only: this is not an upstream response sample.
        const auto profile =
            AccountServiceTestAccess::profileRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, profile,
            R"({"id":900001,"nickname":"一天","avatar_url":"","ip_location":"四川","grade":15})");
        const auto vip =
            AccountServiceTestAccess::vipRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, vip,
            R"({"user_id":900001,"user_type_code":16,"vip_type_code":0,"super_begins_at":"2026-01-02T03:04:05","super_expires_at":"2030-01-02T03:04:05","products":[]})");
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profileNameTags"));
        const auto capture = qEnvironmentVariable("ECHO_TEXT_TAG_PRODUCTION_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                auto *row = visual(session.window->contentItem(), "profileNameTags");
                auto *box = row->property("textBox").value<QQuickItem *>();
                auto *member = visual(row, "profileMembershipTag");
                auto *region = visual(row, "profileRegionTag");
                QVERIFY(box && member && region);
                QTRY_COMPARE(member->property("text").toString(), QStringLiteral("超级VIP"));
                QTRY_COMPARE(row->property("text").toString(), QStringLiteral("一天"));
                QTRY_VERIFY(region->isVisible());
                QCOMPARE(region->property("text").toString(), QStringLiteral("四川"));
                for (auto *tag : {member, region}) {
                    QTRY_VERIFY(qAbs(box->mapToScene({0, box->height() / 2}).y() -
                                     tag->mapToScene({0, tag->height() / 2}).y()) < 0.6);
                }
                if (!capture.isEmpty()) {
                    QTest::qWait(100);
                    QVERIFY(session.window->grabWindow().save(
                        capture + QString("/profile-%1-%2.png")
                                      .arg(mode == ThemeStore::Light ? "light" : "dark")
                                      .arg(zoomed ? "scaled" : "normal")));
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void productionProfileFieldsAndMembershipDates()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        const auto vip =
            AccountServiceTestAccess::vipRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, vip,
            R"({"user_id":900001,"user_type_code":16,"vip_type_code":0,"super_begins_at":"2026-01-02T03:04:05","super_expires_at":"2030-01-02T03:04:05","products":[]})");
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profileChangeAvatar"));
        QTRY_VERIFY(visual(session.window->contentItem(), "profileAccountGroup0"));
        QTRY_VERIFY(visual(session.window->contentItem(), "profileAccountGroup1"));
        auto *basics = visual(session.window->contentItem(), "profileAccountGroup0");
        auto *times = visual(session.window->contentItem(), "profileAccountGroup1");
        QVERIFY(basics && times);
        QTRY_VERIFY(times->y() > basics->y() + basics->height());
        QTRY_COMPARE(visualAll(basics, "profileAccountFieldLabel").size(), 3);
        const auto labels = visualAll(basics, "profileAccountFieldLabel");
        QCOMPARE(labels.size(), 3);
        QTRY_COMPARE(labels.at(0)->property("text").toString(), QStringLiteral("用户 ID"));
        QTRY_COMPARE(labels.at(1)->property("text").toString(), QStringLiteral("性别"));
        QTRY_COMPARE(labels.at(2)->property("text").toString(), QStringLiteral("所在地区"));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                auto *info = visual(session.window->contentItem(), "profileMembershipInfo-super");
                QVERIFY(info && info->isVisible());
                auto *view = visual(session.window->contentItem(), "profileScrollView");
                QVERIFY(view);
                view->setProperty(
                    "contentY",
                    std::max(0., view->property("contentHeight").toReal() - view->height()));
                QTest::qWait(50);
                auto *page = visual(session.window->contentItem(), "profilePage");
                auto *popup = page->findChild<QObject *>("profileMembershipDates");
                QVERIFY(popup);
                const auto point =
                    info->mapToScene({info->width() / 2, info->height() / 2}).toPoint();
                Test::moveNativePointer(session.window, point);
                QTest::mouseClick(session.window, Qt::LeftButton, {}, point);
                QTRY_VERIFY(popup->property("opened").toBool());
                QCOMPARE(
                    session.accounts.account.memberships().first().toMap()["beginsAt"].toString(),
                    QString("2026-01-02 03:04"));
                const auto captures = qEnvironmentVariable("ECHO_PROFILE_PARITY_CAPTURE_DIR");
                if (!captures.isEmpty()) {
                    QVERIFY(QDir().mkpath(captures));
                    QTest::qWait(250);
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/profile-dates-%1-%2.png")
                                       .arg(mode == ThemeStore::Light ? "light" : "dark")
                                       .arg(zoomed ? "scaled" : "normal")));
                }
                QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                QTRY_VERIFY(!popup->property("visible").toBool());
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void productionAccountSurfacesAndAnchors()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        // Typed local catalog display data; no upstream sample or real credentials.
        StoredAccounts rows;
        for (int i = 0; i < 1000; ++i)
            rows.append({quint64(900001 + i), QStringLiteral("账号 %1").arg(i), {}, {}, i});
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        SavedAccountModelTestAccess::present(
            session.accounts.saved, rows,
            AccountServiceTestAccess::snapshot(session.accounts.account));
        const auto profile =
            AccountServiceTestAccess::profileRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, profile,
            R"({"id":900001,"nickname":"Qt 测试账号","avatar_url":"","biography":"正式账号浮层组件的领域呈现测试","grade":5,"gender":1,"province":"上海","city":"上海","visitors":9,"registered_at_seconds":1700000000,"listening_minutes":14409,"friends":0,"follows":5,"fans":1})");
        const auto grade =
            AccountServiceTestAccess::gradeRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, grade,
            R"({"user_id":900001,"grade":5,"listening_seconds":864577,"current_points":17120,"next_grade_points":20000,"next_grade":6})");
        QCOMPARE(session.accounts.saved
                     .data(session.accounts.saved.index(0, 0), SavedAccountModel::DurationRole)
                     .toString(),
                 QStringLiteral("14,409 分钟"));
        const auto vip =
            AccountServiceTestAccess::vipRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, vip,
            R"({"user_id":900001,"user_type_code":0,"vip_type_code":0,"products":[{"active":true,"business":"concept","product":"svip","expires_at":"2030-01-02T03:04:05"}]})");
        QTRY_COMPARE(session.accounts.saved.count(), 1000);
        QCOMPARE(session.accounts.saved
                     .data(session.accounts.saved.index(0, 0), SavedAccountModel::NicknameRole)
                     .toString(),
                 QStringLiteral("Qt 测试账号"));
        auto *panel = session.root->findChild<QObject *>("sidebarAccountPopover");
        auto *switcher = session.root->findChild<QObject *>("accountSwitcherDialog");
        QVERIFY(panel);
        QVERIFY(switcher);
        const auto capture = qEnvironmentVariable("ECHO_ACCOUNT_SURFACES_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool collapsed : {false, true}) {
                if (session.shell.sidebarCollapsed() != collapsed)
                    session.shell.toggleSidebar();
                for (const bool zoomed : {false, true}) {
                    session.shell.resetZoom();
                    if (zoomed)
                        session.shell.zoomIn();
                    if (!collapsed) {
                        auto *meta = visual(session.window->contentItem(), "sidebarAccountMeta");
                        auto *tag = visual(session.window->contentItem(), "sidebarMembershipTag");
                        QVERIFY(meta && tag);
                        auto *label = meta->property("textBox").value<QQuickItem *>();
                        QVERIFY(label);
                        QTRY_VERIFY2(qAbs(label->mapToScene({0, label->height() / 2}).y() -
                                          tag->mapToScene({0, tag->height() / 2}).y()) < 0.5,
                                     qPrintable(QString("label height=%1 y=%2 tag height=%3 y=%4")
                                                    .arg(label->height())
                                                    .arg(label->y())
                                                    .arg(tag->height())
                                                    .arg(tag->y())));
                    }
                    auto *trigger = visual(session.window->contentItem(), "sidebarAccountTrigger");
                    QVERIFY(trigger);
                    if (collapsed) {
                        // The collapsed rail exposes only the avatar profile link.
                        QVERIFY(!trigger->isVisible());
                        auto *avatar = visual(session.window->contentItem(), "sidebarUserLink");
                        QVERIFY(avatar);
                        QTRY_COMPARE(avatar->width(), 42.0);
                        QCOMPARE(avatar->height(), 42.0);
                        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                                          avatar->mapToScene(QPointF(21, 21)).toPoint());
                        QTRY_COMPARE(session.router.current()->name(), QString("profile"));
                        QVERIFY(!panel->property("visible").toBool());
                        continue;
                    }
                    QTRY_COMPARE(trigger->height(), 44.);
                    QVERIFY(QMetaObject::invokeMethod(panel, "open"));
                    QTRY_VERIFY(panel->property("opened").toBool());
                    auto *content = panel->property("contentItem").value<QQuickItem *>();
                    QVERIFY(content);
                    const auto scale = session.shell.uiScale();
                    // Check rendered overlay geometry against its bottom/end anchor and 14px
                    // offset. This case clamps only against the left bound.
                    const auto bottom =
                        trigger->mapToScene(QPointF(trigger->width(), trigger->height()));
                    const auto actual = content->mapToScene(QPointF(0, 0));
                    QVERIFY2(qAbs(actual.y() - (bottom.y() + (14 + 18) * scale)) < 1,
                             qPrintable(QString("panel y=%1 trigger bottom=%2 scale=%3")
                                            .arg(actual.y())
                                            .arg(bottom.y())
                                            .arg(scale)));
                    QVERIFY(qAbs(actual.x() - (12 + 18) * scale) < 1);
                    QVERIFY(!panel->property("modal").toBool());
                    auto *profileAction = visual(content, "accountProfileAction");
                    auto *switchAction = visual(content, "accountSwitchAction");
                    auto *logoutAction = visual(content, "accountLogoutAction");
                    QVERIFY(profileAction && switchAction && logoutAction);
                    QTRY_VERIFY(qAbs(profileAction->width() - switchAction->width()) < 1);
                    QVERIFY(qAbs(logoutAction->width() - switchAction->width()) < 1);
                    auto *background = panel->property("background").value<QQuickItem *>();
                    QVERIFY(background);
                    if (!capture.isEmpty() && !zoomed) {
                        session.window->update();
                        QTest::qWait(100);
                        QVERIFY(session.window->grabWindow().save(
                            capture + QString("/account-popover-%1-%2.png")
                                          .arg(mode == ThemeStore::Light ? "light" : "dark")
                                          .arg(collapsed ? "rail" : "full")));
                    }
                    QVERIFY(QMetaObject::invokeMethod(panel, "close"));
                    QTRY_VERIFY(!panel->property("visible").toBool());
                }
            }
            session.shell.resetZoom();
            QVERIFY(QMetaObject::invokeMethod(switcher, "open"));
            QTRY_VERIFY(switcher->property("opened").toBool());
            auto *add = visual(session.window->contentItem(), "accountAddButton");
            auto *manage = visual(session.window->contentItem(), "accountManageButton");
            auto *badge = visual(session.window->contentItem(), "dialogHeadingBadge");
            auto *close = visual(session.window->contentItem(), "dialogCloseButton");
            auto *list = visual(session.window->contentItem(), "savedAccountList");
            QVERIFY(add && manage && badge && close && list);
            QTRY_COMPARE(add->height(), 32.);
            QCOMPARE(manage->height(), add->height());
            QCOMPARE(badge->property("count").toInt(), 1000);
            auto *currentTag = visual(list, "currentAccountTag");
            QVERIFY(currentTag);
            QCOMPARE(currentTag->property("text").toString(), QStringLiteral("当前登录"));
            QCOMPARE(badge->height(), 18.);
            QVERIFY(badge->width() >= 18.);
            const auto left = switcher->property("x").toReal();
            const auto top = switcher->property("y").toReal();
            const auto closePosition = close->mapToScene(QPointF(0, 0));
            QVERIFY(qAbs(closePosition.x() + close->width() -
                         (left + switcher->property("width").toReal() - 24)) < 1);
            QVERIFY(qAbs(closePosition.y() - (top + switcher->property("topPadding").toReal())) <
                    1);
            QTRY_VERIFY(list->height() > 100);
            QTRY_VERIFY(visualCount(list, "savedAccountRow") > 0);
            QVERIFY(visualCount(list, "savedAccountRow") < 30);
            list->setProperty("contentY",
                              list->property("contentHeight").toReal() - list->height());
            QTest::qWait(60);
            QVERIFY(visualCount(list, "savedAccountRow") < 30);
            list->setProperty("contentY", 0);
            if (!capture.isEmpty()) {
                QTest::qWait(100);
                QVERIFY(session.window->grabWindow().save(
                    capture + QString("/account-switcher-%1.png")
                                  .arg(mode == ThemeStore::Light ? "light" : "dark")));
            }
            QVERIFY(QMetaObject::invokeMethod(switcher, "close"));
            QTRY_VERIFY(!switcher->property("visible").toBool());
        }
        auto *picker = session.root->findChild<QObject *>("sidebarShortcutPopup");
        QVERIFY(picker);
        const auto captures = qEnvironmentVariable("ECHO_ACCOUNT_SURFACES_CAPTURE_DIR");
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool collapsed : {false, true}) {
                if (session.shell.sidebarCollapsed() != collapsed)
                    session.shell.toggleSidebar();
                QTest::qWait(300); // Source sidebar width animation stays enabled.
                QVERIFY(QMetaObject::invokeMethod(picker, "open"));
                QTRY_VERIFY(picker->property("opened").toBool());
                const auto edge = picker->property("anchorRect").toRectF().right();
                const auto expected = session.window->width() - edge - 20 >= 340 ? edge + 8 : 12;
                QTRY_VERIFY(qAbs(picker->property("x").toReal() - expected) < 1);
                auto *content = picker->property("contentItem").value<QQuickItem *>();
                QVERIFY(content);
                QVERIFY(visual(content, "shortcutArtistsHeading"));
                QVERIFY(visual(content, "shortcutPlaylistsHeading"));
                if (!captures.isEmpty()) {
                    QTest::qWait(100);
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/shortcut-picker-%1-%2.png")
                                       .arg(mode == ThemeStore::Light ? "light" : "dark")
                                       .arg(collapsed ? "rail" : "full")));
                }
                QVERIFY(QMetaObject::invokeMethod(picker, "close"));
                QTRY_VERIFY(!picker->property("visible").toBool());
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void profileEditorKeepsEqualColumnsAndDateActionsClear()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 900);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profileEdit"));
        auto *page = visual(session.window->contentItem(), "profilePage");
        QTRY_VERIFY(page->findChild<QObject *>("profileEditorDialog"));
        auto *editor = page->findChild<QObject *>("profileEditorDialog");
        QVERIFY(QMetaObject::invokeMethod(editor, "open"));
        QTRY_VERIFY(editor->property("opened").toBool());
        QTRY_VERIFY(visual(session.window->contentItem(), "profileEditBirthday"));
        QTRY_VERIFY(visual(session.window->contentItem(), "profileEditGender"));
        auto *birthday = visual(session.window->contentItem(), "profileEditBirthday");
        auto *gender = visual(session.window->contentItem(), "profileEditGender");
        auto *day = visual(birthday, "dateDay");
        auto *year = visual(birthday, "dateYear");
        auto *clear = visual(birthday, "dateClear");
        auto *calendar = visual(birthday, "dateCalendar");
        QVERIFY(day && year && clear && calendar);
        QSignalSpy picked(birthday, SIGNAL(picked(QString)));
        const auto captures = qEnvironmentVariable("ECHO_PROFILE_EDITOR_CAPTURE_DIR");
        if (!captures.isEmpty())
            QVERIFY(QDir().mkpath(captures));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                for (const int width : {520, 440}) {
                    editor->setProperty("preferredWidth", width);
                    for (const bool populated : {false, true}) {
                        editor->setProperty("draftBirthday", populated ? "2000-09-28" : "");
                        year->forceActiveFocus();
                        QTRY_VERIFY(qAbs(gender->width() - birthday->width()) < 1);
                        QTRY_COMPARE(birthday->property("hasDate").toBool(), populated);
                        if (populated) {
                            QTRY_COMPARE(year->property("text").toString(), QString("2000"));
                            QTRY_COMPARE(day->property("text").toString(), QString("28"));
                        }
                        const auto *firstAction = populated ? clear : calendar;
                        QTRY_VERIFY(day->mapToItem(birthday, {day->width(), 0}).x() <=
                                    firstAction->mapToItem(birthday, QPointF()).x());
                        QTRY_VERIFY(calendar->mapToItem(birthday, {calendar->width(), 0}).x() <=
                                    birthday->width());
                        QVERIFY(QMetaObject::invokeMethod(year, "editingFinished"));
                        QCOMPARE(picked.size(), 0); // Merely focusing never changes the draft date.
                        if (!captures.isEmpty()) {
                            QTest::qWait(100);
                            QVERIFY(session.window->grabWindow().save(
                                captures + QString("/editor-%1-%2-%3-%4.png")
                                               .arg(mode == ThemeStore::Light ? "light" : "dark")
                                               .arg(zoomed ? "125" : "100")
                                               .arg(width)
                                               .arg(populated ? "date" : "empty-focused")));
                        }
                    }
                }
            }
        }
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_VERIFY(!editor->property("visible").toBool());
        QCOMPARE(warnings.size(), 0);
    }
    void liveContentBlacklistThroughQtService()
    {
        if (qEnvironmentVariable("ECHO_LIVE_CONTENT_BLACKLIST") != "1")
            QSKIP("Explicit current-account temporary blacklist roundtrip only");
        const std::string input((std::istreambuf_iterator<char>(std::cin)),
                                std::istreambuf_iterator<char>());
        const auto identity = QJsonDocument::fromJson(QByteArray::fromStdString(input)).object();
        DeviceIdentity device;
        device.mid = identity.value("mid").toString();
        device.dfid = identity.value("dfid").toString();
        device.guid = identity.value("guid").toString();
        device.mac = identity.value("mac").toString();
        device.deviceName = identity.value("device_name").toString();
        ApiClient api(device);
        AccountService accountService(api);
        QVERIFY(AccountServiceTestAccess::adopt(
            accountService, QJsonDocument(QJsonObject{{"user_id", identity.value("userid")},
                                                      {"token", identity.value("token")},
                                                      {"refresh_credential", identity.value("t1")},
                                                      {"nickname", ""},
                                                      {"avatar_url", ""}})
                                .toJson()));
        ContentBlacklistService service(api, accountService);
        QSignalSpy succeeded(&api, &ApiClient::succeeded), failed(&api, &ApiClient::failed);
        const auto waitFor = [&](quint64 id) {
            QElapsedTimer elapsed;
            elapsed.start();
            while (elapsed.elapsed() < 30000) {
                for (const auto &args : failed)
                    if (args[0].toULongLong() == id)
                        return false;
                for (const auto &args : succeeded)
                    if (args[0].toULongLong() == id)
                        return true;
                QTest::qWait(10);
            }
            return false;
        };
        for (int category = 0; category < 2; ++category) {
            service.selectCategory(category);
            service.activate();
            QTRY_VERIFY_WITH_TIMEOUT(service.loaded() && !service.loading(), 30000);
            // Require the freshly observed empty baseline; never overwrite an
            // existing user's blocked resource or delete unrelated entries.
            QCOMPARE(service.entries()->count(), 0);
            QVERIFY(service.error().isEmpty());
            QCOMPARE(category == 0 ? service.songCount() : service.singerCount(), QString("0"));
            const auto kind =
                category == 0 ? ApiClient::BlacklistKind::Song : ApiClient::BlacklistKind::Singer;
            const auto key = category == 0 ? QString("105a2705beee36a8bed029a8e1a6311d")
                                           : identity.value("public_artist_id").toString();
            QVERIFY(!key.isEmpty());
            const auto name = QStringLiteral("EchoMusicNext blacklist verification");
            bool mayExist = true;
            const auto cleanup = qScopeGuard([&] {
                if (mayExist)
                    waitFor(api.editContentBlacklist(kind, key, name, "32042818", true));
            });
            const auto add = api.editContentBlacklist(kind, key, name, "32042818", false);
            QVERIFY(add);
            QVERIFY(waitFor(add));
            service.refresh();
            QTRY_VERIFY_WITH_TIMEOUT(!service.loading() && !service.entries()->entry(key).isEmpty(),
                                     30000);
            QCOMPARE(service.entries()->count(), 1);
            QVERIFY(service.error().isEmpty());
            service.requestRemoval(key);
            QVERIFY(service.pendingRemoval());
            QSignalSpy removed(&service, &ContentBlacklistService::removalFinished);
            service.confirmRemoval();
            QVERIFY(service.removing());
            QTRY_COMPARE_WITH_TIMEOUT(removed.size(), 1, 30000);
            QVERIFY(removed.front()[0].toBool());
            QVERIFY(!service.pendingRemoval());
            service.refresh();
            QTRY_VERIFY_WITH_TIMEOUT(!service.loading(), 30000);
            QCOMPARE(service.entries()->count(), 0);
            QVERIFY(service.entries()->entry(key).isEmpty());
            QVERIFY(service.error().isEmpty());
            mayExist = false;
            service.deactivate();
        }
        service.shutdown();
        api.shutdown();
    }
    void songMenuBlacklistUsesIndexLoadAndSharedWriteChannel()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService accountService(api);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        ContentBlacklistService service(api, accountService);
        QSignalSpy finished(&service, &ContentBlacklistService::menuWriteFinished);
        const QString key = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        // 索引未加载时为 Unknown；菜单命令先完整加载索引。
        QCOMPARE(service.songStatus(key), ContentBlacklistService::SongStatus::Unknown);
        service.requestSongBlacklist(key, "歌曲 A", "1", false);
        QVERIFY(service.menuBusy());
        const auto read = ContentBlacklistTestAccess::songIndexRequest(service);
        QVERIFY(read);
        const auto readRequest = ApiClientTestAccess::request(api, read);
        QCOMPARE(readRequest.value("operation").toString(), QString("blacklist_list"));
        QCOMPARE(readRequest.value("query").toObject().value("label").toString(), QString("song"));
        QCOMPARE(readRequest.value("query").toObject().value("page").toInt(), 1);
        QCOMPARE(readRequest.value("query").toObject().value("page_size").toInt(), 500);
        // 读取默认允许自动验证；写入路径永远不自动验证（ApiClient 写策略）。
        QVERIFY(ApiClientTestAccess::automaticVerification(api, read));
        ContentBlacklistTestAccess::complete(
            service, api, read,
            R"({"page":1,"page_size":500,"total":1,"items":[{"key":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","name":"歌曲 B","mix_song_id":"2"}]})");
        // 目标不在列表 → 直接进入写入通道。
        const auto write = ContentBlacklistTestAccess::menuWrite(service);
        QVERIFY(write);
        const auto writeRequest = ApiClientTestAccess::request(api, write);
        QCOMPARE(writeRequest.value("operation").toString(), QString("blacklist"));
        const auto query = writeRequest.value("query").toObject();
        QCOMPARE(query.value("label").toString(), QString("song"));
        QCOMPARE(query.value("key").toString(), key);
        QCOMPARE(query.value("name").toString(), QString("歌曲 A"));
        QCOMPARE(query.value("mix_song_id").toString(), QString("1"));
        QVERIFY(!query.value("remove").toBool());
        QVERIFY(!ApiClientTestAccess::automaticVerification(api, write));
        ContentBlacklistTestAccess::complete(service, api, write, R"({"updated":true})");
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.first().at(0).toBool(), true);
        QCOMPARE(finished.first().at(1).toBool(), false);
        QCOMPARE(finished.first().at(2).toString(), QString("已标记为不感兴趣"));
        QCOMPARE(service.songStatus(key), ContentBlacklistService::SongStatus::Present);
        QVERIFY(!service.menuBusy());
        // 重复添加只显示提示，不重复写入。
        service.requestSongBlacklist(key, "歌曲 A", "1", false);
        QCOMPARE(finished.size(), 2);
        QCOMPARE(finished.last().at(1).toBool(), true);
        QCOMPARE(finished.last().at(2).toString(), QString("已标记为不感兴趣，可在个人中心管理"));
        QCOMPARE(ContentBlacklistTestAccess::menuWrite(service), quint64(0));
        // 取消不感兴趣走同一写入通道。
        service.requestSongBlacklist(key, "歌曲 A", "1", true);
        const auto remove = ContentBlacklistTestAccess::menuWrite(service);
        QVERIFY(remove);
        QVERIFY(ApiClientTestAccess::request(api, remove)
                    .value("query")
                    .toObject()
                    .value("remove")
                    .toBool());
        ContentBlacklistTestAccess::complete(service, api, remove, R"({"updated":true})");
        QCOMPARE(finished.size(), 3);
        QCOMPARE(finished.last().at(2).toString(), QString("已撤销不感兴趣"));
        QCOMPARE(service.songStatus(key), ContentBlacklistService::SongStatus::Absent);
        // 账号切换清空索引与待决意图，不把旧账号判定带入新会话。
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900002)));
        QCOMPARE(service.songStatus(key), ContentBlacklistService::SongStatus::Unknown);
        service.requestSongBlacklist(key, "歌曲 A", "1", false);
        QVERIFY(ContentBlacklistTestAccess::songIndexRequest(service));
        service.shutdown();
        api.shutdown();
    }
    void contentBlacklistPaginationRemovalAndSessionIsolation()
    {
        ApiClient api(DeviceIdentity::temporary());
        AccountService accountService(api);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        ContentBlacklistService service(api, accountService);
        QSignalSpy finished(&service, &ContentBlacklistService::removalFinished);
        service.activate();
        auto read = ContentBlacklistTestAccess::read(service, api, 0);
        ContentBlacklistTestAccess::complete(
            service, api, read,
            R"({"page":1,"page_size":2,"total":5,"items":[{"key":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","name":"歌曲 A","mix_song_id":"1"},{"key":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","name":"歌曲 B"}]})");
        QVERIFY(service.loaded());
        QVERIFY(service.hasMore());
        QCOMPARE(service.entries()->count(), 2);
        service.loadMore();
        read = ContentBlacklistTestAccess::request(service, 0);
        QCOMPARE(
            ApiClientTestAccess::request(api, read).value("query").toObject().value("page").toInt(),
            2);
        ContentBlacklistTestAccess::complete(
            service, api, read,
            R"({"page":2,"page_size":2,"total":5,"items":[{"key":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","name":"歌曲 B 更新"},{"key":"cccccccccccccccccccccccccccccccc","name":"歌曲 C"}]})");
        QCOMPARE(service.entries()->count(), 3);
        service.requestRemoval("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        QVERIFY(service.pendingRemoval());
        service.cancelRemoval();
        QVERIFY(!service.pendingRemoval());
        QVERIFY(!service.removing());
        service.requestRemoval("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        service.confirmRemoval();
        auto write = ContentBlacklistTestAccess::write(service);
        QVERIFY(write);
        QVERIFY(!ApiClientTestAccess::automaticVerification(api, write));
        QVERIFY(ApiClientTestAccess::request(api, write)
                    .value("query")
                    .toObject()
                    .value("remove")
                    .toBool());
        service.confirmRemoval();
        QCOMPARE(ContentBlacklistTestAccess::write(service), write);
        ContentBlacklistTestAccess::fail(service, api, write);
        QCOMPARE(service.entries()->count(), 3);
        QVERIFY(service.pendingRemoval());
        QCOMPARE(finished.size(), 1);
        service.confirmRemoval();
        write = ContentBlacklistTestAccess::write(service);
        ContentBlacklistTestAccess::complete(service, api, write, R"({"updated":true})");
        QVERIFY(!service.pendingRemoval());
        QCOMPARE(service.entries()->count(), 2);
        QCOMPARE(service.songCount(), QString("4"));
        // A successful removal from an offset page realigns from page one.
        read = ContentBlacklistTestAccess::request(service, 0);
        QVERIFY(read);
        QCOMPARE(
            ApiClientTestAccess::request(api, read).value("query").toObject().value("page").toInt(),
            1);
        ContentBlacklistTestAccess::complete(
            service, api, read,
            R"({"page":1,"page_size":30,"total":2,"items":[{"key":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","name":"歌曲 B"},{"key":"cccccccccccccccccccccccccccccccc","name":"歌曲 C"}]})");
        service.requestRemoval("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
        service.confirmRemoval();
        write = ContentBlacklistTestAccess::write(service);
        service.deactivate();
        service.activate();
        const auto overlap = ContentBlacklistTestAccess::request(service, 0);
        QVERIFY(overlap);
        ContentBlacklistTestAccess::complete(service, api, write, R"({"updated":true})");
        ContentBlacklistTestAccess::complete(
            service, api, overlap,
            R"({"page":1,"page_size":30,"total":2,"items":[{"key":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb","name":"过期读取不能恢复已移除项"}]})");
        QCOMPARE(service.entries()->count(), 1);
        QCOMPARE(finished.size(), 2); // No late closed-dialog toast.
        service.activate();
        read = ContentBlacklistTestAccess::read(service, api, 0);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900002)));
        ContentBlacklistTestAccess::complete(
            service, api, read,
            R"({"page":1,"page_size":30,"total":1,"items":[{"key":"old-account","name":"迟到的旧账号资料"}]})");
        QCOMPARE(service.entries()->count(), 0);
        QVERIFY(!service.pendingRemoval());
        QCOMPARE(service.songCount(), QString());
        service.shutdown();
        api.shutdown();
    }
    void productionContentBlacklistKeepsPreferencesDraftAndVirtualScroll()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 900);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profilePage"));
        auto *page = visual(session.window->contentItem(), "profilePage");
        QTRY_VERIFY(page->findChild<QObject *>("listeningPreferencesDialog"));
        auto *preferences = page->findChild<QObject *>("listeningPreferencesDialog");
        auto *blacklist = page->findChild<QObject *>("contentBlacklistDialog");
        QVERIFY(blacklist);
        auto &preferenceService = *session.profile.preferences();
        auto prefRead = ListeningPreferencesTestAccess::read(preferenceService, session.api);
        ListeningPreferencesTestAccess::complete(preferenceService, prefRead, preferencesDomain());
        QVERIFY(QMetaObject::invokeMethod(preferences, "open"));
        QTRY_VERIFY(preferences->property("opened").toBool());
        prefRead = ListeningPreferencesTestAccess::read(preferenceService, session.api);
        ListeningPreferencesTestAccess::complete(preferenceService, prefRead, preferencesDomain());
        preferenceService.setStrength("L1", 20);
        QVERIFY(preferenceService.dirty());
        QTRY_VERIFY(visual(session.window->contentItem(), "preferencesBlacklist"));
        auto *trigger = visual(session.window->contentItem(), "preferencesBlacklist");
        QVERIFY(trigger->isEnabled());
        QMetaObject::invokeMethod(trigger, "clicked");
        QTRY_VERIFY(blacklist->property("opened").toBool());
        auto &service = *session.profile.blacklist();
        const auto populate = [&](int category, int count) {
            QJsonArray items;
            for (int i = 0; i < count; ++i)
                items.append(QJsonObject{
                    {"key", category == 0 ? QString("%1").arg(i + 1, 32, 16, QLatin1Char('0'))
                                          : QString::number(i + 1)},
                    {"name", i == 0
                                 ? QStringLiteral(
                                       "很长的中文歌曲名称 Long Latin fallback title for elision")
                                 : QString("呈现条目 %1").arg(i + 1)},
                    {"created_at", "1700000000"}});
            const auto read = ContentBlacklistTestAccess::read(service, session.api, category);
            ContentBlacklistTestAccess::complete(
                service, session.api, read,
                QJsonDocument(
                    QJsonObject{{"page", 1}, {"page_size", 30}, {"total", count}, {"items", items}})
                    .toJson());
        };
        auto *list = visual(session.window->contentItem(), "contentBlacklistList");
        QVERIFY(list);
        const auto captures = qEnvironmentVariable("ECHO_BLACKLIST_CAPTURE_DIR");
        if (!captures.isEmpty())
            QVERIFY(QDir().mkpath(captures));
        const auto held = ContentBlacklistTestAccess::hold(service, session.api, 0);
        QTRY_VERIFY(visual(session.window->contentItem(), "contentBlacklistState"));
        QTRY_COMPARE(visual(session.window->contentItem(), "contentBlacklistState")
                         ->property("text")
                         .toString(),
                     QStringLiteral("正在加载"));
        if (!captures.isEmpty()) {
            QTest::qWait(80);
            QVERIFY(session.window->grabWindow().save(captures + "/blacklist-loading.png"));
        }
        ContentBlacklistTestAccess::fail(service, session.api, held);
        QTRY_VERIFY(visual(session.window->contentItem(), "contentBlacklistRetry"));
        auto *retry = visual(session.window->contentItem(), "contentBlacklistRetry");
        QTRY_VERIFY(retry->isVisible());
        if (!captures.isEmpty()) {
            QTest::qWait(80);
            QVERIFY(session.window->grabWindow().save(captures + "/blacklist-error.png"));
        }
        QMetaObject::invokeMethod(retry, "clicked");
        QVERIFY(service.loading());
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                service.selectCategory(0);
                populate(0, 1000);
                QTRY_VERIFY(visualCount(list, "contentBlacklistRow") > 0);
                QVERIFY(visualCount(list, "contentBlacklistRow") < 30);
                verifyPopupScrollMapping(session.window, list);
                QVERIFY(!QTest::currentTestFailed());
                if (!captures.isEmpty()) {
                    QTest::qWait(80);
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/blacklist-%1-%2.png")
                                       .arg(mode == ThemeStore::Light ? "light" : "dark")
                                       .arg(zoomed ? "125" : "100")));
                }
                QTRY_VERIFY(visual(list, "contentBlacklistUndo"));
                QMetaObject::invokeMethod(visual(list, "contentBlacklistUndo"), "clicked");
                QTRY_VERIFY(service.pendingRemoval());
                auto *confirmation =
                    blacklist->findChild<QObject *>("contentBlacklistConfirmation");
                QVERIFY(confirmation);
                QTRY_VERIFY(confirmation->property("opened").toBool());
                if (!captures.isEmpty()) {
                    QTest::qWait(80);
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/blacklist-confirm-%1-%2.png")
                                       .arg(mode == ThemeStore::Light ? "light" : "dark")
                                       .arg(zoomed ? "125" : "100")));
                }
                service.cancelRemoval();
                QTRY_VERIFY(!confirmation->property("visible").toBool());
                service.selectCategory(1);
                populate(1, 0);
                QTRY_VERIFY(visual(session.window->contentItem(), "contentBlacklistState"));
                QCOMPARE(service.singerCount(), QString("0"));
                auto *tabs = visual(session.window->contentItem(), "contentBlacklistCategories");
                auto *selection = visual(tabs, "tabSelection");
                QVERIFY(selection);
                QTRY_VERIFY(qAbs(selection->x() - (tabs->property("inset").toReal() +
                                                   tabs->property("tabWidth").toReal() + 4)) < 0.5);
                if (!captures.isEmpty()) {
                    QTest::qWait(80);
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/blacklist-empty-%1-%2.png")
                                       .arg(mode == ThemeStore::Light ? "light" : "dark")
                                       .arg(zoomed ? "125" : "100")));
                }
            }
        }
        QVERIFY(QMetaObject::invokeMethod(blacklist, "close"));
        QTRY_VERIFY(!blacklist->property("visible").toBool());
        QVERIFY(preferences->property("visible").toBool());
        QVERIFY(preferenceService.dirty());
        QTRY_COMPARE(list->property("count").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(preferences, "close"));
        QCOMPARE(warnings.size(), 0);
    }
    void deviceRemovalRequiresConfirmationRefreshAndCurrentSession()
    {
        ApiClient api;
        AccountService accountService(api);
        ProfileService profile(api, accountService);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900001)));
        QSignalSpy notices(&profile, &ProfileService::deviceRemovalFinished);
        const auto device = [](const QString &id, bool current) {
            return QJsonObject{{"id", id},
                               {"current", current},
                               {"model", "EchoMusicNext"},
                               {"platform", "PC"},
                               {"active_at_seconds", qint64(1700000000)},
                               {"app_id", 3116},
                               {"client_version", 11440},
                               {"transport_fingerprint", "domain-fingerprint"}};
        };
        const QJsonArray rows{device("current", true), device("other", false)};
        const auto populate = [&](const QJsonArray &data) {
            const auto read = ProfileServiceTestAccess::deviceRequest(profile, api);
            ProfileServiceTestAccess::complete(profile, read, QJsonDocument(data).toJson());
        };
        populate(rows);
        profile.activateDevices();
        profile.requestDeviceRemoval("current");
        QVERIFY(!profile.pendingDeviceRemoval());
        profile.requestDeviceRemoval("other");
        QVERIFY(profile.pendingDeviceRemoval());
        QCOMPARE(profile.pendingDeviceTitle(), QStringLiteral("EchoMusicNext"));
        QVERIFY(!profile.removingDevice());
        profile.cancelDeviceRemoval();
        QVERIFY(!profile.pendingDeviceRemoval());
        profile.confirmDeviceRemoval();
        QVERIFY(!profile.removingDevice());
        profile.requestDeviceRemoval("other");
        profile.confirmDeviceRemoval();
        const auto first = ProfileServiceTestAccess::deviceKick(profile);
        QVERIFY(first);
        const auto request = ApiClientTestAccess::request(api, first);
        QCOMPARE(request.value("operation").toString(), QStringLiteral("login_device_kick"));
        QCOMPARE(request.value("query").toObject().value("target_mid").toString(),
                 QStringLiteral("other"));
        QVERIFY(!ApiClientTestAccess::automaticVerification(api, first));
        profile.confirmDeviceRemoval();
        QCOMPARE(ProfileServiceTestAccess::deviceKick(profile), first);
        ProfileServiceTestAccess::fail(profile, first, "{}");
        QVERIFY(!profile.removingDevice());
        QVERIFY(profile.pendingDeviceRemoval());
        QCOMPARE(profile.devices()->count(), 2);
        QVERIFY(!profile.devicesError().isEmpty());
        profile.confirmDeviceRemoval();
        const auto retry = ProfileServiceTestAccess::deviceKick(profile);
        const auto oldRead = ProfileServiceTestAccess::deviceRequest(profile, api);
        ProfileServiceTestAccess::complete(profile, retry, R"({"removed":true})");
        QVERIFY(profile.removingDevice());
        QVERIFY(profile.pendingDeviceRemoval()); // Keep confirmation busy through the forced read.
        const auto refresh = ProfileServiceTestAccess::deviceRead(profile);
        QVERIFY(refresh && refresh != oldRead);
        ProfileServiceTestAccess::complete(profile, oldRead, QJsonDocument(rows).toJson());
        QCOMPARE(ProfileServiceTestAccess::deviceRead(profile), refresh);
        ProfileServiceTestAccess::complete(profile, refresh,
                                           QJsonDocument(QJsonArray{rows[0]}).toJson());
        QVERIFY(!profile.removingDevice());
        QCOMPARE(profile.devices()->count(), 1);
        QVERIFY(notices.last()[0].toBool());
        populate(rows);
        profile.requestDeviceRemoval("other");
        profile.confirmDeviceRemoval();
        const auto hidden = ProfileServiceTestAccess::deviceKick(profile);
        const auto noticeCount = notices.size();
        profile.deactivateDevices();
        QVERIFY(!profile.pendingDeviceRemoval());
        ProfileServiceTestAccess::fail(profile, hidden, "{}");
        QCOMPARE(notices.size(), noticeCount);
        profile.activateDevices();
        profile.requestDeviceRemoval("other");
        profile.confirmDeviceRemoval();
        ProfileServiceTestAccess::complete(profile, ProfileServiceTestAccess::deviceKick(profile),
                                           R"({"removed":true})");
        ProfileServiceTestAccess::fail(profile, ProfileServiceTestAccess::deviceRead(profile),
                                       R"({"kind":"authentication","code":20018})");
        QVERIFY(!profile.removingDevice());
        QVERIFY(!profile.pendingDeviceRemoval());
        QVERIFY(profile.devicesError().contains(QStringLiteral("重新登录")));
        QCOMPARE(profile.devices()->count(), 2); // Failed refresh never fabricates an empty list.
        profile.requestDeviceRemoval("other");
        profile.confirmDeviceRemoval();
        const auto late = ProfileServiceTestAccess::deviceKick(profile);
        QVERIFY(AccountServiceTestAccess::adopt(accountService, account(900002)));
        ProfileServiceTestAccess::complete(profile, late, R"({"removed":true})");
        QCOMPARE(profile.devices()->count(), 0);
        QVERIFY(!profile.removingDevice());
        QCOMPARE(notices.size(), noticeCount);
    }
    void profileDevicesKeepInlineStatusAndBoundedViewport()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 1000);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profilePage"));
        auto *page = visual(session.window->contentItem(), "profilePage");
        QTRY_VERIFY(page->findChild<QObject *>("profileDeviceDialog"));
        auto *dialog = page->findChild<QObject *>("profileDeviceDialog");
        // Canonical service-domain presentation input, not upstream wire samples.
        const auto populate = [&](int count) {
            QJsonArray rows;
            for (int index = 0; index < count; ++index)
                rows.append(QJsonObject{
                    {"id", QString("domain-device-%1").arg(index)},
                    {"model", index == 2 ? QStringLiteral(
                                               "很长的中文设备名称 Long Latin fallback device name")
                                         : QString("Device %1").arg(index + 1)},
                    {"platform", "Android"},
                    {"location", QStringLiteral("中国,南京")},
                    {"login_type", 6},
                    {"app_id", 3116},
                    {"client_version", 11440},
                    {"transport_fingerprint", "domain-fingerprint"},
                    {"active_at_seconds", 1700000000},
                    {"new_at_seconds", index == 1 ? 1700000000 : 1690000000},
                    {"current", index == 0}});
            const auto request =
                ProfileServiceTestAccess::deviceRequest(session.profile, session.api);
            ProfileServiceTestAccess::complete(session.profile, request,
                                               QJsonDocument(rows).toJson(QJsonDocument::Compact));
        };
        populate(9);
        QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        QTRY_VERIFY(visual(session.window->contentItem(), "profileDeviceList"));
        auto *list = visual(session.window->contentItem(), "profileDeviceList");
        const auto captures = qEnvironmentVariable("ECHO_PROFILE_DEVICES_CAPTURE_DIR");
        if (!captures.isEmpty())
            QVERIFY(QDir().mkpath(captures));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                QTRY_VERIFY(visualAll(list, "profileDeviceRow").size() >= 3);
                QTRY_VERIFY(list->height() > 200);
                verifyPopupScrollMapping(session.window, list);
                QVERIFY(!QTest::currentTestFailed());
                QVERIFY(dialog->property("height").toReal() <= 720);
                QVERIFY(dialog->property("height").toReal() <=
                        session.window->height() / session.shell.uiScale() - 140 + 1);
                auto *summary = visual(session.window->contentItem(), "profileDeviceSummary");
                QVERIFY(summary);
                QTRY_COMPARE(summary->property("text").toString(),
                             QStringLiteral("当前账号已登录 9 台设备"));
                for (auto *row : visualAll(list, "profileDeviceRow")) {
                    auto *title = visual(row, "profileDeviceTitle");
                    auto *tag = visual(row, "profileDeviceStatus");
                    auto *remove = visual(row, "profileDeviceRemove");
                    QVERIFY(title && tag && remove);
                    auto *textBox = title->property("textBox").value<QQuickItem *>();
                    QVERIFY(textBox);
                    if (tag->isVisible()) {
                        // A short name must keep its tag beside the rendered text;
                        // long names reserve the tag and the separate action column.
                        QTRY_VERIFY(
                            qAbs(tag->mapToItem(title, QPointF()).x() - textBox->width() - 8) < 1);
                        QVERIFY(tag->mapToItem(row, {tag->width(), 0}).x() <= row->width() - 13);
                        if (!row->property("currentDevice").toBool()) {
                            QCOMPARE(tag->property("text").toString(), QStringLiteral("新设备"));
                            QCOMPARE(tag->property("customTone").value<QColor>(),
                                     QColor("#10b981"));
                        }
                    }
                    if (remove->isVisible()) {
                        QVERIFY(remove->isEnabled());
                        QCOMPARE(remove->height(), 36.0);
                        QTRY_VERIFY(title->mapToItem(row, {title->width(), 0}).x() + 11 <=
                                    remove->mapToItem(row, QPointF()).x());
                    }
                }
                if (!captures.isEmpty()) {
                    QTest::qWait(100);
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/devices-%1-%2.png")
                                       .arg(mode == ThemeStore::Light ? "light" : "dark")
                                       .arg(zoomed ? "125" : "100")));
                }
                list->setProperty("contentY", 0);
                QTest::qWait(100);
                QQuickItem *removeAction = nullptr;
                for (auto *row : visualAll(list, "profileDeviceRow")) {
                    if (row->property("currentDevice").toBool() || row->y() < 0 ||
                        row->y() + row->height() > list->height())
                        continue;
                    removeAction = visual(row, "profileDeviceRemove");
                    break;
                }
                QVERIFY(removeAction && removeAction->isEnabled());
                const auto point =
                    removeAction
                        ->mapToScene({removeAction->width() / 2, removeAction->height() / 2})
                        .toPoint();
                Test::moveNativePointer(session.window, point);
                QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
                QTRY_VERIFY(session.profile.pendingDeviceRemoval());
                auto *confirmation = dialog->findChild<QObject *>("profileDeviceConfirmation");
                QVERIFY(confirmation);
                QTRY_VERIFY(confirmation->property("opened").toBool());
                if (!captures.isEmpty())
                    QVERIFY(session.window->grabWindow().save(
                        captures + QString("/device-confirm-%1-%2.png")
                                       .arg(mode == ThemeStore::Light ? "light" : "dark")
                                       .arg(zoomed ? "125" : "100")));
                session.profile.cancelDeviceRemoval();
                QTRY_VERIFY(!confirmation->property("visible").toBool());
            }
        }
        // A small list shrinks to native row geometry; a large list remains virtual.
        populate(2);
        QTRY_COMPARE(list->property("count").toInt(), 2);
        if (!captures.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(session.window->grabWindow().save(captures + "/devices-two.png"));
        }
        QTRY_VERIFY2(list->height() >= 160 && list->height() < 180,
                     qPrintable(QString("height=%1 implicit=%2 measured=%3 max=%4 mounted=%5")
                                    .arg(list->height())
                                    .arg(list->implicitHeight())
                                    .arg(list->property("measuredExtent").toDouble())
                                    .arg(list->property("maximumViewportHeight").toDouble())
                                    .arg(visualCount(list, "profileDeviceRow"))));
        auto *bar = popupScrollBarFor(session.window, list);
        QVERIFY(bar);
        QTRY_VERIFY(!bar->isVisible());
        populate(1000);
        QTRY_COMPARE(list->property("count").toInt(), 1000);
        QTRY_VERIFY(visualCount(list, "profileDeviceRow") < 30);
        list->setProperty("contentY", list->property("contentHeight").toReal() - list->height());
        QTest::qWait(100);
        QVERIFY(visualCount(list, "profileDeviceRow") < 30);
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QTRY_COMPARE(list->property("count").toInt(), 0);
        QCOMPARE(warnings.size(), 0);
    }
    void profileRelatedScrollBarsUseNativeMapping()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 900);
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profilePage"));
        auto *page = visual(session.window->contentItem(), "profilePage");
        const auto social =
            ProfileServiceTestAccess::socialRequest(session.profile, session.api, 0);
        ProfileServiceTestAccess::complete(session.profile, social, socialRows(1000));
        auto &preferences = *session.profile.preferences();
        const auto read = ListeningPreferencesTestAccess::read(preferences, session.api);
        ListeningPreferencesTestAccess::complete(preferences, read, preferencesDomain());
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    for (int step = 0; step < 5; ++step)
                        session.shell.zoomIn();
                const QList<QPair<QString, QString>> surfaces{
                    {"profileEditorDialog", "profileEditorScroll"},
                    {"listeningPreferencesDialog", "listeningPreferencesScroll"},
                    {"profileSocialDrawer", "profileSocialList"}};
                for (const auto &[name, scrollingView] : surfaces) {
                    QTRY_VERIFY(page->findChild<QObject *>(name));
                    auto *surface = page->findChild<QObject *>(name);
                    if (name != "profileSocialDrawer")
                        surface->setProperty("maximumHeight", 420);
                    QVERIFY(QMetaObject::invokeMethod(surface, "open"));
                    QTRY_VERIFY(surface->property("opened").toBool());
                    QTRY_VERIFY(visual(session.window->contentItem(), scrollingView));
                    auto *view = visual(session.window->contentItem(), scrollingView);
                    verifyPopupScrollMapping(session.window, view);
                    QVERIFY(!QTest::currentTestFailed());
                    QVERIFY(QMetaObject::invokeMethod(surface, "close"));
                    QTRY_VERIFY(!surface->property("visible").toBool());
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void productionProfileSettingsAndSidebarOverlays()
    {
        Session session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->resize(1150, 800);
        QVERIFY(Test::prepareNativeInput(session.window));
        QTRY_VERIFY(visual(session.window->contentItem(), "profilePage"));
        QVERIFY(AccountServiceTestAccess::adopt(session.accounts.account, account(900001)));
        const auto profile =
            AccountServiceTestAccess::profileRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, profile,
            R"({"id":900001,"nickname":"Qt 测试账号","avatar_url":"","biography":"正式个人中心组件的领域呈现测试","grade":5,"gender":1,"province":"上海","city":"上海","visitors":9,"registered_at_seconds":1700000000,"listening_minutes":14409,"friends":0,"follows":5,"fans":1})");
        const auto grade =
            AccountServiceTestAccess::gradeRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, grade,
            R"({"user_id":900001,"grade":5,"listening_seconds":864577,"current_points":17120,"next_grade_points":20000,"next_grade":6})");
        const auto membership =
            AccountServiceTestAccess::vipRequest(session.accounts.account, session.api);
        AccountServiceTestAccess::complete(
            session.accounts.account, membership,
            R"({"user_id":900001,"user_type_code":0,"vip_type_code":0,"products":[{"active":true,"business":"concept","product":"svip","expires_at":"2030-01-02T03:04:05"}]})");
        session.router.invalidateAccount();
        session.router.navigate("profile");
        QTRY_VERIFY(visual(session.window->contentItem(), "profileSwitchAccount"));
        auto *profilePage = visual(session.window->contentItem(), "profilePage");
        QVERIFY(profilePage);
        auto *editor = profilePage->findChild<QObject *>("profileEditorDialog");
        auto *social = profilePage->findChild<QObject *>("profileSocialDrawer");
        auto *devices = profilePage->findChild<QObject *>("profileDeviceDialog");
        QVERIFY(editor);
        QVERIFY(social);
        QVERIFY(devices);
        auto *gradeDialog = profilePage->findChild<QObject *>("profileGradeDialog");
        QVERIFY(gradeDialog);
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const bool zoomed : {false, true}) {
                session.shell.resetZoom();
                if (zoomed)
                    session.shell.zoomIn();
                QVERIFY(QMetaObject::invokeMethod(gradeDialog, "open"));
                QTRY_VERIFY(gradeDialog->property("opened").toBool());
                auto *card = visual(session.window->contentItem(), "profileGradeCard");
                auto *outline = visual(session.window->contentItem(), "profileGradeCardOutline");
                auto *planet = visual(session.window->contentItem(), "profileGradePlanet");
                auto *track = visual(session.window->contentItem(), "profileGradeTrack");
                auto *refresh = visual(session.window->contentItem(), "profileGradeRefresh");
                auto *close = gradeDialog->property("closeButton").value<QQuickItem *>();
                QVERIFY(card && outline && planet && track && refresh && close);
                QTRY_VERIFY(card->height() >= 176);
                QCOMPARE(outline->size(), card->size());
                QCOMPARE(QQmlProperty(outline, "border.width").read().toReal(), 1.0);
                const auto border = QQmlProperty(outline, "border.color").read().value<QColor>();
                QVERIFY(qAbs(border.alphaF() - 0.72) < 0.01);
                QTRY_COMPARE(track->height(), 5.0);
                const auto refreshBackground =
                    refresh->property("background").value<QQuickItem *>();
                QVERIFY(refreshBackground);
                QCOMPARE(QQmlProperty(refreshBackground, "border.width").read().toReal(), 0.0);
                QCOMPARE(close->size(), QSizeF(32, 32));
                const auto scale = session.shell.uiScale();
                const auto edge = close->mapToScene(QPointF(close->width(), 0));
                QTRY_VERIFY(
                    qAbs(edge.x() - (gradeDialog->property("x").toReal() +
                                     (gradeDialog->property("width").toReal() - 24) * scale)) < 1);
                QVERIFY(qAbs(edge.y() - (gradeDialog->property("y").toReal() +
                                         gradeDialog->property("topPadding").toReal() * scale)) <
                        1);
                QTRY_VERIFY(qAbs(planet->x() + planet->width() - planet->parentItem()->width()) <
                            1);
                const auto captureDir = qEnvironmentVariable("ECHO_DESKTOP_PAGES_CAPTURE_DIR");
                if (!captureDir.isEmpty()) {
                    QVERIFY(QDir().mkpath(captureDir));
                    QVERIFY(session.window->grabWindow().save(
                        captureDir + QString("/grade-%1-%2.png")
                                         .arg(mode == ThemeStore::Light ? "light" : "dark")
                                         .arg(zoomed ? "125" : "100")));
                }
                QVERIFY(Test::prepareNativeInput(session.window));
                QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                                  close->mapToScene(QPointF(16, 16)).toPoint());
                QTRY_VERIFY(!gradeDialog->property("visible").toBool());
            }
        }
        session.shell.resetZoom();
        QVERIFY(QMetaObject::invokeMethod(editor, "open"));
        QTRY_VERIFY(editor->property("opened").toBool());
        QCOMPARE(editor->property("draftName").toString(), session.accounts.account.nickname());
        auto *birthday = visual(session.window->contentItem(), "profileEditBirthday");
        QVERIFY(birthday);
        QVERIFY(birthday->property("value").toString().isEmpty());
        QSignalSpy picked(birthday, SIGNAL(picked(QString)));
        auto *year = birthday->findChild<QQuickItem *>("dateYear");
        QVERIFY(year);
        year->forceActiveFocus();
        QVERIFY(QMetaObject::invokeMethod(year, "editingFinished"));
        QCOMPARE(picked.size(), 0); // Focusing an empty birthday must not write today.
        const auto capture = qEnvironmentVariable("ECHO_DESKTOP_PAGES_CAPTURE_DIR");
        if (!capture.isEmpty()) {
            QDir().mkpath(capture);
            session.window->update();
            QTest::qWait(120);
            QVERIFY(session.window->grabWindow().save(capture + "/profile-editor.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(editor, "close"));
        QTRY_VERIFY(!editor->property("visible").toBool());
        const auto friends =
            ProfileServiceTestAccess::socialRequest(session.profile, session.api, 0);
        ProfileServiceTestAccess::complete(session.profile, friends, socialRows(1000));
        QVERIFY(QMetaObject::invokeMethod(social, "open"));
        QTRY_VERIFY(social->property("opened").toBool());
        auto *socialHeader = visual(session.window->contentItem(), "profileSocialHeader");
        QVERIFY(socialHeader);
        QTRY_VERIFY(socialHeader->width() >= social->property("width").toReal());
        auto *socialClose = visual(session.window->contentItem(), "profileSocialClose");
        QVERIFY(socialClose);
        QTRY_VERIFY(
            qAbs(socialClose->mapToItem(socialHeader, QPointF(socialClose->width(), 0)).x() -
                 (socialHeader->width() - 24)) < 1);
        auto *list = visual(session.window->contentItem(), "profileSocialList");
        QVERIFY(list);
        QTRY_VERIFY(visualCount(list, "profileSocialRow") > 0);
        QVERIFY(visualCount(list, "profileSocialRow") < 40);
        QCOMPARE(list->property("count").toInt(), 1000);
        QTRY_COMPARE(visualAll(list, "userIdentityBadge").size(), 3);
        auto *socialAvatar = visual(list, "socialAccountAvatar");
        QVERIFY(socialAvatar);
        QVERIFY(!socialAvatar->property("identityIcon").toUrl().isEmpty());
        if (!capture.isEmpty()) {
            QTest::qWait(80);
            QVERIFY(session.window->grabWindow().save(capture + "/profile-social-identities.png"));
        }
        list->setProperty("contentY", list->property("contentHeight").toReal() - list->height());
        QTest::qWait(60);
        QVERIFY(visualCount(list, "profileSocialRow") < 40);
        auto *reusedCard = visual(list, "profileSocialCard");
        QVERIFY(reusedCard);
        QCOMPARE(reusedCard->x(), 12.);
        QCOMPARE(reusedCard->width(), list->width() - 24.);
        // Retained/pooled delegates are allowed to keep display metadata. They
        // must not keep image work active or leak the first row into reused rows.
        int activeRows = 0;
        for (auto *row : visualAll(list, "profileSocialRow")) {
            auto *reusedAvatar = visual(row, "socialAccountAvatar");
            QVERIFY(reusedAvatar);
            auto *mark = reusedAvatar->findChild<QQuickItem *>("avatarIdentityMark");
            QVERIFY(mark);
            if (row->property("resourceActive").toBool()) {
                ++activeRows;
                QVERIFY(reusedAvatar->property("identityIcon").toUrl().isEmpty());
                QCOMPARE(visualAll(row, "userIdentityBadge").size(), 0);
            } else {
                QTRY_VERIFY(mark->property("source").toUrl().isEmpty());
            }
        }
        QVERIFY(activeRows > 0);
        if (!capture.isEmpty()) {
            // Earlier grade captures finish in dark mode. Explicitly select
            // light here so the two social captures cover distinct palettes.
            session.theme.setMode(ThemeStore::Light);
            QTest::qWait(80);
            QVERIFY(session.window->grabWindow().save(capture + "/profile-social.png"));
            session.theme.setMode(ThemeStore::Dark);
            QTest::qWait(120);
            QVERIFY(session.window->grabWindow().save(capture + "/profile-social-dark.png"));
            session.theme.setMode(ThemeStore::Light);
        }
        QVERIFY(QMetaObject::invokeMethod(social, "close"));
        QTRY_VERIFY(!social->property("visible").toBool());
        QTRY_COMPARE(list->property("count").toInt(), 0);
        const auto deviceRead =
            ProfileServiceTestAccess::deviceRequest(session.profile, session.api);
        ProfileServiceTestAccess::complete(
            session.profile, deviceRead,
            R"([{"id":"domain-local","model":"设备 A","platform":"Android","location":"中国,上海","login_type":6,"active_at_seconds":1700000000,"new_at_seconds":1700000000,"current":true},{"id":"domain-other","model":"设备 B","platform":"Android","location":"中国,北京","login_type":1,"active_at_seconds":1690000000,"new_at_seconds":1680000000,"current":false}])");
        QVERIFY(QMetaObject::invokeMethod(devices, "open"));
        QTRY_VERIFY(devices->property("opened").toBool());
        QTRY_COMPARE(visualCount(session.window->contentItem(), "profileDeviceRow"), 2);
        auto *deviceList = visual(session.window->contentItem(), "profileDeviceList");
        QVERIFY(deviceList);
        QTRY_VERIFY(deviceList->height() >= 160);
        if (!capture.isEmpty()) {
            QTest::qWait(80);
            QVERIFY(session.window->grabWindow().save(capture + "/profile-devices.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(devices, "close"));
        QTRY_VERIFY(!devices->property("visible").toBool());
        auto *preferences = profilePage->findChild<QObject *>("listeningPreferencesDialog");
        QVERIFY(preferences);
        auto &preferenceService = *session.profile.preferences();
        const auto prefRead = ListeningPreferencesTestAccess::read(preferenceService, session.api);
        ListeningPreferencesTestAccess::complete(preferenceService, prefRead, preferencesDomain());
        preferenceService.setStrength("L1", 20);
        QVERIFY(QMetaObject::invokeMethod(preferences, "open"));
        QTRY_VERIFY(preferences->property("opened").toBool());
        QCOMPARE(preferences->property("width").toReal(), 640.);
        auto *preferenceScroll =
            visual(session.window->contentItem(), "listeningPreferencesScroll");
        QVERIFY(preferenceScroll);
        QTRY_VERIFY(preferenceScroll->height() > 350);
        auto *strength = visual(session.window->contentItem(), "preferenceStrengthL1");
        QVERIFY(strength);
        QVERIFY(strength->width() > 150);
        QVERIFY(strength->isEnabled());
        QPointer<QQuickItem> heldStrength(strength);
        preferenceService.setStrength("L1", 40);
        QTest::qWait(20);
        QVERIFY(heldStrength);
        QCOMPARE(visual(session.window->contentItem(), "preferenceStrengthL1"), strength);
        const auto first =
            strength->mapToScene(QPointF(strength->width() * 0.25, strength->height() / 2))
                .toPoint();
        const auto second =
            strength->mapToScene(QPointF(strength->width() * 0.75, strength->height() / 2))
                .toPoint();
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, first);
        QTest::mouseMove(session.window, second, 20);
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, second);
        QVERIFY(heldStrength);
        if (!capture.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(session.window->grabWindow().save(capture + "/listening-preferences.png"));
            session.theme.setMode(ThemeStore::Dark);
            QTest::qWait(120);
            QVERIFY(session.window->grabWindow().save(capture + "/listening-preferences-dark.png"));
            // Exercise the bounded option grid in both palettes. Widths select
            // five, three and two columns without resizing the application.
            preferences->setProperty("moreExpanded", true);
            preferenceService.toggleLanguage("19");
            preferenceService.toggleLanguage("22");
            preferenceService.toggleStyle("2");
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                session.theme.setMode(mode);
                for (const auto width : {640., 520., 400.}) {
                    preferences->setProperty("width", width);
                    for (int tab = 0; tab < 2; ++tab) {
                        preferences->setProperty("moreTab", tab);
                        QTest::qWait(100);
                        preferenceScroll->setProperty(
                            "contentY", preferenceScroll->property("contentHeight").toReal() -
                                            preferenceScroll->height());
                        QTest::qWait(150);
                        QVERIFY(session.window->grabWindow().save(
                            capture + QString("/preference-options-%1-%2-%3.png")
                                          .arg(tab == 0 ? "languages" : "styles")
                                          .arg(mode == ThemeStore::Light ? "light" : "dark")
                                          .arg(width)));
                    }
                }
            }
            preferenceService.toggleLanguage("19");
            preferenceService.toggleLanguage("22");
            preferenceService.toggleStyle("2");
            preferences->setProperty("width", 640.);
            preferences->setProperty("moreExpanded", false);
            preferences->setProperty("moreTab", 0);
            preferenceScroll->setProperty("contentY", 0.);
            session.theme.setMode(ThemeStore::Light);
        }
        QVERIFY(strength->property("value").toDouble() > 65);
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, QPoint(5, 5));
        auto *discard = preferences->findChild<QObject *>("listeningDiscardConfirm");
        QVERIFY(discard);
        QTRY_VERIFY(discard->property("opened").toBool());
        QVERIFY(preferences->property("visible").toBool());
        QTest::keyClick(session.window, Qt::Key_Escape);
        QTRY_VERIFY(!discard->property("visible").toBool());
        preferenceService.discard();
        QVERIFY(QMetaObject::invokeMethod(preferences, "attemptClose"));
        QTRY_VERIFY(!preferences->property("visible").toBool());
        session.router.navigate("settings");
        auto *dialog = session.root->findChild<QObject *>("settingsDialog");
        QVERIFY(dialog);
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto *catalog = dialog->property("catalog").value<SettingsCatalog *>();
        QVERIFY(catalog);
        QTRY_VERIFY(visual(session.window->contentItem(), "settingsDefaultSearch"));
        const auto parent = dialog->property("parent").value<QQuickItem *>();
        QVERIFY(parent);
        QCOMPARE(dialog->property("width").toReal(),
                 qMin(820.0, parent->width() - (parent->width() <= 640 ? 16 : 48)));
        QCOMPARE(dialog->property("height").toReal(),
                 qMin(620.0, parent->height() - (parent->width() <= 640 ? 32 : 96)));
        catalog->select("font");
        QTRY_VERIFY(visual(session.window->contentItem(), "settingsGlobalFont"));
        QTRY_VERIFY(!visual(session.window->contentItem(), "settingsDefaultSearch"));
        catalog->select("experimental");
        catalog->setFilter("no-such-option");
        QTRY_VERIFY(!visual(session.window->contentItem(), "settingsGlobalFont"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
        session.router.navigate("settings");
        QTRY_COMPARE(catalog->current(), QString("interface"));
        QTRY_VERIFY(visual(session.window->contentItem(), "settingsDefaultSearch"));
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        auto *layout = session.root->findChild<QObject *>("sidebarLayoutPopup");
        QVERIFY(layout);
        QVERIFY(QMetaObject::invokeMethod(layout, "open"));
        QTRY_VERIFY(layout->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(layout, "close"));
        QTRY_VERIFY(!layout->property("visible").toBool());
        session.theme.setMode(ThemeStore::Dark);
        const auto directory = qEnvironmentVariable("ECHO_DESKTOP_PAGES_CAPTURE_DIR");
        if (!directory.isEmpty()) {
            QDir().mkpath(directory);
            session.window->resize(1150, 800);
            session.window->update();
            QTest::qWait(200);
            QVERIFY(session.window->grabWindow().save(directory + "/profile-dark.png"));
            // Cached route pages are visually parented by RouteHost, while the
            // QObject owner belongs to the route loader rather than Main.qml.
            auto *profilePage = visual(session.window->contentItem(), "profilePage");
            QVERIFY(profilePage);
            auto *gradeDialog = profilePage->findChild<QObject *>("profileGradeDialog");
            QVERIFY(gradeDialog);
            QVERIFY(QMetaObject::invokeMethod(gradeDialog, "open"));
            QTRY_VERIFY(gradeDialog->property("opened").toBool());
            auto *planet = visual(session.window->contentItem(), "profileGradePlanet");
            QVERIFY(planet);
            // The hero uses space-between; SVG intrinsic dimensions must not
            // leave the illustration beside the level label.
            QTRY_VERIFY(qAbs(planet->x() + planet->width() - planet->parentItem()->width()) < 1);
            QTest::qWait(100);
            QVERIFY(session.window->grabWindow().save(directory + "/grade-dark.png"));
            QVERIFY(QMetaObject::invokeMethod(gradeDialog, "close"));
            QTRY_VERIFY(!gradeDialog->property("visible").toBool());
            session.router.navigate("settings");
            QTRY_VERIFY(dialog->property("opened").toBool());
            QTRY_VERIFY(visual(session.window->contentItem(), "settingsDefaultSearch"));
            QTest::qWait(80);
            QVERIFY(session.window->grabWindow().save(directory + "/settings-dark.png"));
            session.theme.setMode(ThemeStore::Light);
            QTest::qWait(80);
            QVERIFY(session.window->grabWindow().save(directory + "/settings-light.png"));
        }
        QCOMPARE(warnings.size(), 0);
    }
};
int main(int argc, char **argv)
{
    initializeNetworkEnvironment();
    // Match the production entry point for translucent secondary windows.
    QQuickWindow::setDefaultAlphaBuffer(true);
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("EchoMusicTests");
    QCoreApplication::setApplicationName("DesktopPages");
    configureUiTypography();
    QQuickStyle::setStyle("Basic");
    DesktopPagesTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "DesktopPagesTest.moc"
