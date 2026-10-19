#include "Application.h"
#include "StartupController.h"
#include "ApplicationUrlHandler.h"
#include "PluginRuntime.h"
#include "PluginManager.h"
#include "SingleInstance.h"
#include "RelaunchService.h"
#include "RouteController.h"
#include "SearchModel.h"
#include "SearchSuggestionModel.h"
#include "SearchRouteCoordinator.h"
#include "DiscoveryService.h"
#include "RecognitionService.h"
#include "ExploreService.h"
#include "PlaylistDetailService.h"
#include "LibraryCollectionService.h"
#include "AlbumDetailService.h"
#include "DetailsService.h"
#include "ArtistDetailService.h"
#include "HistoryService.h"
#include "FavoritesService.h"
#include "CloudService.h"
#include "PurchasedService.h"
#include "DiscoveryRouteCoordinator.h"
#include "ListenTogetherUi.h"
#include "ShareResolveService.h"
#include "SharingCommands.h"
#include <QUrlQuery>
#include "ProfileRouteCoordinator.h"
#include "ProfileService.h"
#include "ShellSettings.h"
#include "SidebarLayoutModel.h"
#include "ShellController.h"
#include "SongCommands.h"
#include "ShortcutService.h"
#include "ShortcutActions.h"
#include "AboutService.h"
#include "SettingsDataService.h"
#include "PlaylistImportTaskCoordinator.h"
#include "ThemeStore.h"
#include "WindowController.h"
#include "AccountService.h"
#include "QrLoginService.h"
#include "AccountController.h"
#include "LibraryController.h"
#include "SidebarRouteCoordinator.h"
#include "SavedAccountModel.h"
#include "ApiClient.h"
#include "PlaylistMutationService.h"
#include "UserPlaylistModel.h"
#include "DeviceRegistrationService.h"
#include "NetworkPreferences.h"
#include "LogService.h"
#include "StorageService.h"
#include "PlaybackService.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QProcess>
#include <memory>
#include <utility>

namespace
{
int runRecoveryFailure(QGuiApplication &app, QSettings &settings, const QString &error,
                       EchoMusic::SingleInstance *instance, const QStringList &arguments)
{
    // Do not construct account stores, transport, plugins or audio against an
    // unfinished restore. The retry uses the same fully drained relaunch helper.
    EchoMusic::ThemeStore theme(settings);
    EchoMusic::ShellSettings shell(settings);
    EchoMusic::WindowController window(settings, false);
    theme.setMotionEnabled(shell.motionEnabled());
    QObject::connect(&theme, &EchoMusic::ThemeStore::changed, &window, [&] {
        window.setAppearance(theme.windowFrosted(), theme.dark(), theme.transparency());
    });
    EchoMusic::StartupController startup(error);
    EchoMusic::RelaunchService relaunch(instance ? instance->lockFilePath() : QString(), arguments,
                                        false);
    QQmlApplicationEngine engine;
    theme.attachToEngine(engine);
    const auto quit = [&] {
        startup.shutdown();
        window.shutdown();
        app.quit();
    };
    QObject::connect(&startup, &EchoMusic::StartupController::quitRequested, &engine, quit);
    QObject::connect(&startup, &EchoMusic::StartupController::restartApplicationRequested,
                     &relaunch, &EchoMusic::RelaunchService::restartForStartupRecovery);
    QObject::connect(&relaunch, &EchoMusic::RelaunchService::notice, &startup,
                     &EchoMusic::StartupController::reportRestartFailure);
    QObject::connect(&relaunch, &EchoMusic::RelaunchService::readyToQuit, &engine, quit);
    QObject::connect(&window, &EchoMusic::WindowController::quitRequested, &engine, quit);
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &engine, quit);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &engine, quit);
    engine.setInitialProperties({{"startup", QVariant::fromValue(&startup)},
                                 {"windowService", QVariant::fromValue(&window)},
                                 {"shellSettings", QVariant::fromValue(&shell)}});
    engine.loadFromModule("EchoMusic.App", "StartupWindow");
    if (engine.rootObjects().isEmpty())
        return 1;
    window.attach(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));
    window.setAppearance(theme.windowFrosted(), theme.dark(), theme.transparency());
    if (instance)
        QObject::connect(instance, &EchoMusic::SingleInstance::activationRequested, &window,
                         &EchoMusic::WindowController::showWindow);
    startup.start();
    const int result = app.exec();
    qDeleteAll(engine.rootObjects());
    return result;
}
} // namespace

int EchoMusic::Application::run()
{
    auto &app = m_app;
    const bool startupSmoke = app.arguments().contains(QStringLiteral("--startup-smoke-test"));
    const bool smoke = startupSmoke || app.arguments().contains(QStringLiteral("--smoke-test"));
    const bool useStartup = !smoke || startupSmoke;
    QTemporaryDir smokeDirectory;
    auto preferences = smoke ? std::make_unique<QSettings>(smokeDirectory.path() +
                                                               QStringLiteral("/preferences.ini"),
                                                           QSettings::IniFormat)
                             : std::make_unique<QSettings>();
    auto &settings = *preferences;
    const auto applicationData =
        smoke ? smokeDirectory.path()
              : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const auto restoreError = EchoMusic::SettingsDataService::recover(applicationData, settings);
    if (!restoreError.isEmpty()) {
        qCritical().noquote() << "Cannot recover settings restore:" << restoreError;
        return runRecoveryFailure(app, settings, restoreError, m_instance,
                                  RelaunchService::persistentArguments(app.arguments()));
    }
    EchoMusic::ThemeStore theme(settings, nullptr,
                                smoke ? smokeDirectory.path() + QStringLiteral("/theme-images")
                                      : QString());
    theme.pruneImageAssets();
    EchoMusic::ShellSettings shell(settings);
    theme.setMotionEnabled(shell.motionEnabled());
    QObject::connect(&shell, &EchoMusic::ShellSettings::changed, &theme,
                     [&] { theme.setMotionEnabled(shell.motionEnabled()); });
    EchoMusic::WindowController window(settings, !smoke);
    QObject::connect(&theme, &EchoMusic::ThemeStore::changed, &window, [&] {
        window.setAppearance(theme.windowFrosted(), theme.dark(), theme.transparency());
    });
    bool activatedDuringStartup = false;
    if (m_instance)
        QObject::connect(m_instance, &SingleInstance::activationRequested, &window, [&] {
            activatedDuringStartup = true;
            window.showWindow();
            qInfo("Instance activation: main window restored");
        });
    EchoMusic::RouteController router;
    router.setHomeLayout(shell.sidebarLayout());
    EchoMusic::ShellController shellController(settings, router);
    // Imports/share URLs are one-shot actions and must not execute again after
    // relaunch. Preserve explicit runtime diagnostics and trusted-plugin mode.
    EchoMusic::RelaunchService relaunch(m_instance ? m_instance->lockFilePath() : QString(),
                                        RelaunchService::persistentArguments(app.arguments()),
                                        ECHO_DISTRIBUTION_BUILD && !smoke);
    QObject::connect(&relaunch, &EchoMusic::RelaunchService::notice, &shellController,
                     [&](const QString &message) { shellController.toasts()->show(message); });
    if (m_logging)
        QObject::connect(m_logging, &LogService::notice, &shellController,
                         [&](const QString &message) { shellController.toasts()->show(message); });
    shellController.attachTheme(theme);
    router.setCachePolicy(shell.keepAliveEnabled(), shell.keepAliveMax());
    QObject::connect(&shell, &EchoMusic::ShellSettings::changed, &router, [&] {
        router.setCachePolicy(shell.keepAliveEnabled(), shell.keepAliveMax());
    });
    EchoMusic::ApiClient api(smoke ? EchoMusic::DeviceIdentity::temporary()
                                   : EchoMusic::DeviceIdentity::load(settings));
    EchoMusic::NetworkPreferences network(settings, api);
    if (network.transportAvailable())
        network.installApplicationTransport();
    EchoMusic::AboutService about(settings, network);
    QObject::connect(&about, &EchoMusic::AboutService::notice, &shellController,
                     [&](const QString &message, bool failure) {
                         shellController.toasts()->show(message,
                                                        failure ? EchoMusic::ToastService::Warning
                                                                : EchoMusic::ToastService::Info);
                     });
    QObject::connect(&network, &EchoMusic::NetworkPreferences::applied, &shellController, [&] {
        shellController.toasts()->show(QObject::tr("全局网络设置已应用"),
                                       EchoMusic::ToastService::Success);
    });
    QObject::connect(&network, &EchoMusic::NetworkPreferences::notice, &shellController,
                     [&](const QString &message) {
                         shellController.toasts()->show(message, EchoMusic::ToastService::Warning);
                     });
    EchoMusic::PlaybackService playback(
        api, settings, nullptr,
        (smoke ? smokeDirectory.path()
               : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)) +
            QStringLiteral("/playback.sqlite"),
        &network);
    playback.setStartupHeld(useStartup);
    window.attachPlayback(playback);
    EchoMusic::AccountService account(api, nullptr, playback.memberships());
    EchoMusic::ProfileService profile(api, account);
    QObject::connect(&profile, &EchoMusic::ProfileService::deviceRemovalFinished, &shellController,
                     [&](bool success, const QString &message) {
                         shellController.toasts()->show(message,
                                                        success ? EchoMusic::ToastService::Success
                                                                : EchoMusic::ToastService::Danger);
                     });
    QObject::connect(&profile, &EchoMusic::ProfileService::avatarFinished, &shellController,
                     [&](bool success, const QString &message) {
                         shellController.toasts()->show(message,
                                                        success ? EchoMusic::ToastService::Success
                                                                : EchoMusic::ToastService::Danger);
                     });
    QObject::connect(profile.blacklist(), &EchoMusic::ContentBlacklistService::removalFinished,
                     &shellController, [&](bool success, const QString &message) {
                         shellController.toasts()->show(message,
                                                        success ? EchoMusic::ToastService::Success
                                                                : EchoMusic::ToastService::Danger);
                     });
    EchoMusic::UserPlaylistModel userPlaylists(api);
    EchoMusic::PlaylistMutationService playlistMutations(api);
    QObject::connect(&playlistMutations, &EchoMusic::PlaylistMutationService::libraryInvalidated,
                     &userPlaylists, &EchoMusic::UserPlaylistModel::reload);
    EchoMusic::LibraryCollectionService collections(api, userPlaylists, playlistMutations);
    EchoMusic::QrLoginService qrLogin(api);
    EchoMusic::DeviceRegistrationService deviceRegistration(api, settings);
    EchoMusic::StorageService storage;
    userPlaylists.attachStorage(storage);
    EchoMusic::SavedAccountModel savedAccounts(api, account, storage);
    EchoMusic::StartupController startup(storage, api, deviceRegistration, playback, network,
                                         savedAccounts,
                                         applicationData + QStringLiteral("/music.sqlite"));
    EchoMusic::AccountController accountController(api, account, savedAccounts, qrLogin,
                                                   userPlaylists, !smoke);
    QObject::connect(accountController.verification(),
                     &EchoMusic::VerificationService::returnToApplicationRequested, &window,
                     &EchoMusic::WindowController::showWindow);
    EchoMusic::LibraryController libraryController(api, userPlaylists, playlistMutations);
    EchoMusic::SidebarRouteCoordinator sidebarRoutes(libraryController, userPlaylists, router);
    EchoMusic::PlaylistImportTaskCoordinator importTasks(*libraryController.importer(),
                                                         *shellController.tasks(), settings);
    playback.attachLibrary(userPlaylists, playlistMutations);
    playback.listenTogether()->attachAccount(account);
    theme.setCoverColor(playback.coverColor());
    QObject::connect(&playback, &EchoMusic::PlaybackService::changed, &theme,
                     [&] { theme.setCoverColor(playback.coverColor()); });
    shellController.attachPlayback(playback);
    shellController.attachWindows(window);
    // 歌曲菜单共享命令绑定账号与不感兴趣服务；toast 经 ShellController 统一呈现。
    playback.songCommands()->attach(*profile.blacklist(), account);
    EchoMusic::SharingCommands shareCommands(account);
    playback.songCommands()->attachSharing(shareCommands);
    QObject::connect(&shareCommands, &EchoMusic::SharingCommands::notice, &shellController,
                     [&](const QString &message, bool warning) {
                         shellController.toasts()->show(message,
                                                        warning ? EchoMusic::ToastService::Warning
                                                                : EchoMusic::ToastService::Success);
                     });
    EchoMusic::ShortcutService shortcuts(settings, !smoke);
    QObject::connect(&shortcuts, &EchoMusic::ShortcutService::notice, &shellController,
                     [&](const QString &message) {
                         shellController.toasts()->show(message, EchoMusic::ToastService::Warning);
                     });
    EchoMusic::ShortcutActions shortcutActions(shortcuts, playback, shell, window, shellController);
    EchoMusic::SearchModel search(api);
    search.attachPlayback(playback);
    QObject::connect(&search, &EchoMusic::SearchModel::tracksPlayRequested, &playback,
                     &EchoMusic::PlaybackService::playFromSelection);
    QObject::connect(&search, &EchoMusic::SearchModel::tracksAppendRequested, &playback,
                     &EchoMusic::PlaybackService::appendTracks);
    QObject::connect(&search, &EchoMusic::SearchModel::trackPlayNowRequested, &playback,
                     &EchoMusic::PlaybackService::queueAndPlayTrack);
    QObject::connect(&search, &EchoMusic::SearchModel::trackQueueRequested, &playback,
                     [&](const EchoMusic::PlaybackTrack &track, bool cut) {
                         playback.enqueueTracks({track}, cut);
                     });
    search.suggestions()->setDefaultEnabled(shell.searchDefaultEnabled());
    QObject::connect(&shell, &EchoMusic::ShellSettings::changed, &search, [&] {
        search.suggestions()->setDefaultEnabled(shell.searchDefaultEnabled());
    });
    search.setHistoryStore(storage);
    EchoMusic::CommentAuthorService commentAuthors(api, !smoke);
    EchoMusic::SongMetadataService songMetadata(api);
    playback.listenTogether()->attachMetadata(songMetadata);
    EchoMusic::PlaylistDetailService playlistDetails(
        api, playback, userPlaylists, playlistMutations, *libraryController.order(), songMetadata,
        router, !smoke, nullptr, &commentAuthors);
    // 我最喜爱 / 音乐云盘 / 已购音乐的页面数据所有者；各自在构造时注册路由页。
    EchoMusic::FavoritesService favorites(api, playback, userPlaylists, *libraryController.order(),
                                          songMetadata, router, !smoke);
    QObject::connect(libraryController.order(), &EchoMusic::PlaylistOrderModel::notice,
                     &shellController, [&](const QString &message, bool warning) {
                         shellController.toasts()->show(message,
                                                        warning ? EchoMusic::ToastService::Warning
                                                                : EchoMusic::ToastService::Success);
                     });
    QObject::connect(&profile, &EchoMusic::ProfileService::relationshipsMutated, &favorites,
                     &EchoMusic::FavoritesService::invalidateFollow);
    EchoMusic::CloudService cloud(api, playback, songMetadata, router, !smoke);
    cloud.upload()->attachTasks(*shellController.tasks(), router);
    cloud.upload()->setProperty("backgroundReminderMuted",
                                settings.value("cloud/upload/backgroundReminderMuted", false));
    QObject::connect(cloud.upload(), &EchoMusic::CloudUploadService::reminderChanged, &cloud, [&] {
        settings.setValue("cloud/upload/backgroundReminderMuted",
                          cloud.upload()->property("backgroundReminderMuted"));
    });
    QObject::connect(&cloud, &EchoMusic::CloudService::notice, &shellController,
                     [&](const QString &message, bool warning) {
                         shellController.toasts()->show(message,
                                                        warning ? EchoMusic::ToastService::Warning
                                                                : EchoMusic::ToastService::Success);
                     });
    EchoMusic::PurchasedService purchased(api, playback, router, !smoke);
    EchoMusic::DetailsService details(api, playback, router, !smoke, nullptr, &commentAuthors,
                                      &window);
    details.setNetworkPreferences(&network);
    details.attachShell(shellController);
    QObject::connect(&details, &EchoMusic::DetailsService::notice, &shellController,
                     [&](const QString &message, bool warning) {
                         shellController.toasts()->show(message,
                                                        warning ? EchoMusic::ToastService::Warning
                                                                : EchoMusic::ToastService::Success);
                     });
    EchoMusic::AlbumDetailService albumDetails(api, playback, router, !smoke, nullptr,
                                               &commentAuthors);
    playlistDetails.attachCollections(collections);
    albumDetails.attachCollections(collections);
    const auto detailNotice = [&](const QString &message, bool warning) {
        shellController.toasts()->show(message, warning ? EchoMusic::ToastService::Warning
                                                        : EchoMusic::ToastService::Success);
    };
    QObject::connect(&collections, &EchoMusic::LibraryCollectionService::notice, &shellController,
                     detailNotice);
    QObject::connect(&albumDetails, &EchoMusic::AlbumDetailService::notice, &shellController,
                     detailNotice);
    QObject::connect(&playlistDetails, &EchoMusic::PlaylistDetailService::notice, &shellController,
                     detailNotice);
    QObject::connect(details.videoCollection(), &EchoMusic::VideoCollectionService::mutated,
                     &favorites, [&](const QString &, bool) { favorites.invalidateVideos(); });
    EchoMusic::ArtistDetailService artistDetails(api, playback, router, shell, !smoke);
    artistDetails.attachProfile(profile);
    QObject::connect(&profile, &EchoMusic::ProfileService::notice, &shellController, detailNotice);
    QObject::connect(profile.blacklist(), &EchoMusic::ContentBlacklistService::singerWriteFinished,
                     &shellController, [&](bool success, const QString &message) {
                         detailNotice(message, !success);
                     });
    EchoMusic::HistoryService history(api, playback, songMetadata, router, !smoke);
    QObject::connect(history.statsShare(), &EchoMusic::HistoryStatsShare::notice, &shellController,
                     [&](const QString &message, bool failed) {
                         shellController.toasts()->show(message,
                                                        failed ? EchoMusic::ToastService::Danger
                                                               : EchoMusic::ToastService::Success);
                     });
    QObject::connect(&history, &EchoMusic::HistoryService::notice, &shellController,
                     [&](const QString &message) {
                         shellController.toasts()->show(message, EchoMusic::ToastService::Success);
                     });
    QObject::connect(&libraryController, &EchoMusic::LibraryController::playlistRequested, &router,
                     [&](const QString &resourceId, quint64, bool) {
                         router.navigate("playlist-detail", {}, resourceId);
                     });
    EchoMusic::DiscoveryService discovery(api, playback, account);
    EchoMusic::NativeRecognitionCapture recognitionCapture(playback.nativeBridge());
    EchoMusic::RecognitionService recognition(api, playback, recognitionCapture, settings, router);
    EchoMusic::ExploreService explore(api, playback, router, !smoke);
    EchoMusic::DiscoveryRouteCoordinator discoveryRoutes(discovery, router, !smoke);
    EchoMusic::ListenTogetherUi togetherUi(api, playback, userPlaylists, songMetadata, router,
                                           !smoke);
    QObject::connect(playback.listenTogether(), &EchoMusic::ListenTogetherController::notice,
                     &shellController, [&shellController](const QString &message, int level) {
                         shellController.toasts()->show(message,
                                                        EchoMusic::ToastService::Tone(level));
                     });
    QObject::connect(playback.listenTogether(),
                     &EchoMusic::ListenTogetherController::loginRequested, &accountController,
                     &EchoMusic::AccountController::openLogin);
    // Routed discovery pages cannot reach ToastService; the service raises a
    // typed notice and the application forwards it to the shared toast surface.
    QObject::connect(&discovery, &EchoMusic::DiscoveryService::notice, &shellController,
                     [&](const QString &message, int tone) {
                         shellController.toasts()->show(message,
                                                        EchoMusic::ToastService::Tone(tone));
                     });
    EchoMusic::SearchRouteCoordinator searchRoutes(search, router, !smoke);
    EchoMusic::ProfileRouteCoordinator profileRoutes(accountController, profile, router, !smoke);
    router.registerPage({"themes",
                         "/main/themes",
                         QUrl("qrc:/qt/qml/EchoMusic/Themes/ThemeCenterPage.qml"),
                         {},
                         false,
                         false,
                         false,
                         {{"windowService", QVariant::fromValue(&window)}}});
    QQmlApplicationEngine engine;
    theme.attachToEngine(engine);
    const auto pluginRoot =
        (smoke ? smokeDirectory.path()
               : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)) +
        QStringLiteral("/plugins");
    PluginRuntime plugins(engine, settings,
                          {playback, theme, shellController, router, account, api}, pluginRoot,
                          m_pluginSafeMode);
    PluginManager pluginManager(plugins, settings);
    pluginManager.attachSharing(shareCommands);
    EchoMusic::ShareResolveService shareResolver(api, shareCommands, router, shellController,
                                                 window, !smoke);
    shareResolver.setPluginOpener(
        [&](const EchoMusic::ShareTarget &target) { return pluginManager.openShare(target); });
    EchoMusic::SettingsDataService settingsData(settings, plugins, relaunch, network,
                                                applicationData, about.version());
    if (m_instance)
        settingsData.setInstanceLockFilePath(m_instance->lockFilePath());
    settingsData.setMaintenanceGuard([&] {
        return !theme.pendingImageImports() &&
               !playback.audioControls()->importingImpulseResponses() &&
               !playback.audioControls()->providers()->busy() &&
               !playback.effectCatalog()->downloading() && !pluginManager.busy() &&
               !plugins.loading() && !playback.cast()->busy();
    });
    QObject::connect(&settingsData, &EchoMusic::SettingsDataService::notice, &shellController,
                     [&](const QString &message, bool failure) {
                         shellController.toasts()->show(message,
                                                        failure ? EchoMusic::ToastService::Warning
                                                                : EchoMusic::ToastService::Success);
                     });
    pluginManager.setNetworkPreferences(&network);
    QObject::connect(&pluginManager, &PluginManager::notice, &shellController,
                     [&](const QString &message, bool failure, bool informational) {
                         shellController.toasts()->show(message, failure ? ToastService::Warning
                                                                 : informational
                                                                     ? ToastService::Info
                                                                     : ToastService::Success);
                     });
    router.registerPage({"plugin-management",
                         "/main/settings/plugins",
                         QUrl("qrc:/qt/qml/EchoMusic/Plugins/PluginManagementPage.qml"),
                         {"view"},
                         true,
                         false,
                         false,
                         {{"manager", QVariant::fromValue(&pluginManager)}}});
    router.registerPage({"plugin-share-resolve",
                         "/main/settings/plugins/share",
                         QUrl("qrc:/qt/qml/EchoMusic/Plugins/PluginShareResolvePage.qml"),
                         {},
                         false,
                         false,
                         false,
                         {{"manager", QVariant::fromValue(&pluginManager)}}});
    QObject::connect(&pluginManager, &PluginManager::shareOpened, &router,
                     [&] { router.navigate("plugin-share-resolve"); });
    QObject::connect(&pluginManager, &PluginManager::sharedInstalled, &router, [&] {
        if (router.current() && router.current()->name() == "plugin-share-resolve") {
            pluginManager.setSearch(pluginManager.shareRecord().value("id").toString());
            router.navigate("plugin-management", {{"view", "marketplace"}});
        }
    });
    bool urlDeliveryReady = false;
    QList<QUrl> pendingUrls;
    const auto openShared = [&](const QUrl &url) {
        if (!urlDeliveryReady) {
            if (pendingUrls.size() < 16)
                pendingUrls.append(url);
            return;
        }
        window.showWindow();
        if (url.isLocalFile() && url.path().endsWith(".echomod", Qt::CaseInsensitive)) {
            router.navigate("plugin-management");
            if (!pluginManager.installLocalBatch({url}))
                shellController.toasts()->show(QObject::tr("当前正在处理插件，请稍后安装"),
                                               ToastService::Warning);
            return;
        }
        shareResolver.openUrl(url);
    };
    if (m_instance)
        QObject::connect(m_instance, &SingleInstance::urlRequested, &pluginManager, openShared);
    if (m_urlHandler)
        QObject::connect(m_urlHandler, &ApplicationUrlHandler::urlRequested, &pluginManager,
                         openShared);
    QObject::connect(&plugins, &PluginRuntime::pluginFailed, &shellController,
                     [&](const QString &id, const QString &message) {
                         qWarning().noquote() << "Plugin" << id << message;
                     });
    quint64 visibleUserId = account.userId();
    QObject::connect(&account, &EchoMusic::AccountService::changed, &router, [&] {
        if (visibleUserId == account.userId())
            return;
        visibleUserId = account.userId();
        // Adoptions invalidate through sessionAdopted, including same-ID login.
        // This observer handles logout without invalidating a new login twice.
        if (!visibleUserId)
            router.invalidateAccount();
    });
    // QSettings has already restored the sidebar order. Route providers are
    // registered above; unavailable/deferred cards cannot mount a fake page.
    if (!useStartup)
        router.navigateHome();
    QObject::connect(&account, &EchoMusic::AccountService::sessionAdopted, &router, [&] {
        // SQL startup restoration is not an explicit account switch. Recreate
        // account-owned pages without replacing the landing route (or a newer
        // user/deep-link navigation made while storage was loading).
        if (savedAccounts.restoringStartupAccount())
            router.invalidateAccountPreservingRoute();
        else
            router.invalidateAccount();
    });

    bool quitting = false;
    int exitStatus = 0;
    int closedServices = 0;
    const auto beginShutdown = [&] {
        if (std::exchange(quitting, true))
            return;
        startup.shutdown();
        shareResolver.shutdown();
        shortcuts.shutdown();
        about.shutdown();
        settingsData.shutdown();
        pluginManager.shutdown();
        plugins.shutdown();
    };
    QObject::connect(&startup, &EchoMusic::StartupController::quitRequested, &app, beginShutdown);
    QObject::connect(&relaunch, &EchoMusic::RelaunchService::readyToQuit, &app, beginShutdown);
    QObject::connect(&playback, &EchoMusic::PlaybackService::listeningReportsDrained, &api,
                     &EchoMusic::ApiClient::shutdown);
    QObject::connect(&plugins, &PluginRuntime::closed, &app, [&] {
        theme.endBackgroundPreview();
        theme.cancelImageWorkForShutdown();
        cloud.shutdown();
        router.shutdown();
        shellController.shutdown();
        window.shutdown();
        settings.sync();
        recognition.shutdown();
        playback.shutdown();
        discovery.shutdown();
        explore.shutdown();
        playlistDetails.shutdown();
        favorites.shutdown();
        commentAuthors.shutdown();
        details.shutdown();
        albumDetails.shutdown();
        artistDetails.shutdown();
        history.shutdown();
        songMetadata.shutdown();
        search.shutdown();
        accountController.shutdown();
        profile.shutdown();
        savedAccounts.shutdown();
        deviceRegistration.shutdown();
        // Listening events must finish before their immutable API runtime closes.
        // The reporter bounds shutdown and discards uncertain writes without replay.
        // Plugins have drained before their business dependencies close.
        // The account catalog drains local-store writes before closing SQL.
    });
    QObject::disconnect(&engine, &QQmlEngine::quit, &app, nullptr);
    QObject::connect(&engine, &QQmlEngine::quit, &app, beginShutdown);
    const auto serviceClosed = [&] {
        if (++closedServices == 5)
            settingsData.finishShutdown([&](const QString &error) {
                if (!error.isEmpty()) {
                    qCritical().noquote() << "Settings maintenance:" << error;
                    settings.setValue("maintenance/lastError", error);
                    settings.sync();
                    exitStatus = 1;
                }
                app.quit();
            });
    };
    QObject::connect(&playback, &EchoMusic::PlaybackService::closed, &app, serviceClosed);
    QObject::connect(&deviceRegistration, &EchoMusic::DeviceRegistrationService::closed, &app,
                     serviceClosed);
    QObject::connect(&api, &EchoMusic::ApiClient::closed, &app, serviceClosed);
    QObject::connect(&storage, &EchoMusic::StorageService::closed, &app, serviceClosed);
    QObject::connect(&savedAccounts, &EchoMusic::SavedAccountModel::closed, &storage, [&] {
        serviceClosed();
        storage.shutdown();
    });
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app, beginShutdown);
    QObject::connect(&window, &EchoMusic::WindowController::quitRequested, &app, beginShutdown);
    QObject::connect(
        &playback, &EchoMusic::PlaybackService::sleepActionRequested, &app,
        [&](const QString &action) {
            if (action == "quit") {
                beginShutdown();
                return;
            }
            if (action != "shutdown")
                return;
            // Fixed program/arguments match EchoMusic's platform request. No
            // shell, privilege escalation or forced application termination.
            auto *process = new QProcess(&app);
            auto *timeout = new QTimer(process);
            timeout->setSingleShot(true);
            timeout->setInterval(30000);
            QObject::connect(timeout, &QTimer::timeout, process, [process] { process->kill(); });
            auto done = std::make_shared<bool>(false);
            const auto finish = [&, process, timeout, done](bool success) {
                if (std::exchange(*done, true))
                    return;
                timeout->stop();
                playback.finishSleepAction(
                    success ? QString() : QObject::tr("系统未完成关机请求，请检查系统权限"));
                process->deleteLater();
            };
            QObject::connect(process, &QProcess::errorOccurred, &app,
                             [finish](QProcess::ProcessError) { finish(false); });
            QObject::connect(process, &QProcess::finished, &app,
                             [finish](int code, QProcess::ExitStatus status) {
                                 finish(code == 0 && status == QProcess::NormalExit);
                             });
#ifdef Q_OS_MACOS
            process->start("/usr/bin/osascript",
                           {"-e", "tell application \"System Events\" to shut down"});
#elif defined(Q_OS_WIN)
            process->start("shutdown.exe", {"/s", "/t", "0"});
#else
            process->start("/usr/bin/systemctl", {"poweroff"});
#endif
            timeout->start();
        });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [&] {
        exitStatus = 1;
        beginShutdown();
    });
    QObject::connect(&storage, &EchoMusic::StorageService::failed, &app,
                     [&](const QString &message) {
                         qCritical().noquote() << message;
                         if (!useStartup) {
                             exitStatus = 1;
                             beginShutdown();
                         }
                     });

    bool smokeApiReady = false;
    bool pluginStartupReady = false;
    QTimer smokePageProbe;
    const auto finishSmoke = [&] {
        // QML construction can process events before start() imports any package.
        // API readiness alone must not finish an empty plugin catalog smoke.
        if (quitting || !smoke || !smokeApiReady || !pluginStartupReady || plugins.loading() ||
            (useStartup && !startup.entered()))
            return;
        const auto incoming = EchoMusic::parseShareTarget(m_openUrl.toString());
        const bool pluginShare = incoming && incoming->type == EchoMusic::ShareResource::Plugin;
        const auto pageName = m_openUrl.isEmpty() ? QStringLiteral("homePage")
                              : pluginShare       ? QStringLiteral("pluginShareResolvePage")
                                                  : QStringLiteral("shareResolvePage");
        bool pageCreated = false;
        std::function<bool(QQuickItem *)> findPage = [&](QQuickItem *item) {
            if (!item)
                return false;
            if (item->objectName() == pageName)
                return true;
            for (auto *child : item->childItems())
                if (findPage(child))
                    return true;
            return false;
        };
        for (auto *root : engine.rootObjects()) {
            auto *surface = qobject_cast<QQuickWindow *>(root);
            pageCreated = pageCreated || findPage(surface ? surface->contentItem()
                                                          : qobject_cast<QQuickItem *>(root));
        }
        // RouteHost incubates real pages asynchronously. API readiness alone
        // previously let a normal smoke finish before its startup page existed.
        if (!pageCreated)
            return;
        if (!m_openUrl.isEmpty()) {
            if (!router.current() || router.current()->name() !=
                                         (pluginShare ? "plugin-share-resolve" : "share-resolve"))
                exitStatus = 1;
            qInfo("Share smoke: production receiving page created after startup");
        } else {
            if (!router.current() || router.current()->name() != "home")
                exitStatus = 1;
            qInfo("Discovery smoke: production homepage created after startup");
        }
        if (m_openUrl.isEmpty()) {
            QSet<QObject *> visited;
            QList<QObject *> popups;
            QObject *agreement = nullptr;
            QObject *acceptAgreement = nullptr;
            const auto visit = [&](auto &&self, QObject *object) -> void {
                if (!object || visited.contains(object))
                    return;
                visited.insert(object);
                if (object->objectName() == "userAgreementDialog")
                    agreement = object;
                if (object->objectName() == "userAgreementAccept")
                    acceptAgreement = object;
                if (object->inherits("QQuickPopup"))
                    popups.append(object);
                for (auto *child : object->children())
                    self(self, child);
                if (auto *item = qobject_cast<QQuickItem *>(object))
                    for (auto *child : item->childItems())
                        self(self, child);
            };
            for (auto *root : engine.rootObjects())
                visit(visit, root);
            // Fresh smoke preferences exercise the real first-home consent
            // trigger, then audit zero idle popups only after full disposal.
            if (!shell.userAgreementAccepted()) {
                if (!agreement || !agreement->property("opened").toBool() || !acceptAgreement)
                    return;
                if (!QMetaObject::invokeMethod(acceptAgreement, "clicked")) {
                    qCritical("Could not accept first-home terms in smoke");
                    exitStatus = 1;
                    beginShutdown();
                } else {
                    qInfo("First-home terms shown and explicitly accepted in smoke");
                }
                return;
            }
            if (agreement)
                return;
            for (auto *popup : popups)
                qWarning() << "Unexpected idle popup" << popup->objectName()
                           << popup->metaObject()->className();
            qInfo() << "Production homepage idle native Popups:" << popups.size();
            if (!popups.isEmpty())
                exitStatus = 1;
        }
        smokePageProbe.stop();
        for (int row = 0; row < plugins.rowCount(); ++row) {
            const auto state = plugins.data(plugins.index(row), PluginRuntime::StateRole).toInt();
            if (state == PluginRuntime::Failed || state == PluginRuntime::Invalid ||
                state == PluginRuntime::Incompatible)
                exitStatus = 1;
        }
        beginShutdown();
    };
    QObject::connect(&plugins, &PluginRuntime::loadingChanged, &app, finishSmoke);
    if (smoke) {
        smokePageProbe.setInterval(20);
        QObject::connect(&smokePageProbe, &QTimer::timeout, &app, finishSmoke);
        smokePageProbe.start();
        // Smoke uses temporary storage and waits for API, device writer, account
        // account catalog, then SQL to drain in their ownership order.
        if (!useStartup)
            storage.initialize(smokeDirectory.path() + QStringLiteral("/music.sqlite"));
        QObject::connect(&storage, &EchoMusic::StorageService::initialized, &api,
                         [&] { api.capabilities(); });
        QObject::connect(&api, &EchoMusic::ApiClient::succeeded, &app, [&] {
            smokeApiReady = true;
            finishSmoke();
        });
        QObject::connect(&api, &EchoMusic::ApiClient::failed, &app, [&] {
            if (!useStartup) {
                exitStatus = 1;
                beginShutdown();
            }
        });
        QObject::connect(&startup, &EchoMusic::StartupController::changed, &app, [&] {
            if (useStartup && startup.failed()) {
                qCritical().noquote() << "Startup smoke:" << startup.statusMessage();
                exitStatus = 1;
                beginShutdown();
            }
        });
        QTimer::singleShot(startupSmoke ? 60000 : 10000, &app, [&] {
            qCritical("Smoke timed out");
            exitStatus = 1;
            beginShutdown();
        });
    }
    shellController.bindBarrageSettings(shell);
    engine.setInitialProperties(
        {{QStringLiteral("startup"), QVariant::fromValue(useStartup ? &startup : nullptr)},
         {QStringLiteral("pluginManager"), QVariant::fromValue(&pluginManager)},
         {QStringLiteral("shortcutService"), QVariant::fromValue(&shortcuts)},
         {QStringLiteral("aboutService"), QVariant::fromValue(&about)},
         {QStringLiteral("settingsDataService"), QVariant::fromValue(&settingsData)},
         {QStringLiteral("networkPreferences"), QVariant::fromValue(&network)},
         {QStringLiteral("logService"), QVariant::fromValue(m_logging)},
         {QStringLiteral("relaunchService"), QVariant::fromValue(&relaunch)},
         {QStringLiteral("playbackDiscussion"), QVariant::fromValue(details.playbackDiscussion())},
         {QStringLiteral("playbackService"), QVariant::fromValue(&playback)},
         {QStringLiteral("libraryController"), QVariant::fromValue(&libraryController)},
         {QStringLiteral("accountController"), QVariant::fromValue(&accountController)},
         {QStringLiteral("shellController"), QVariant::fromValue(&shellController)},
         {QStringLiteral("searchService"), QVariant::fromValue(&search)},
         {QStringLiteral("router"), QVariant::fromValue(&router)},
         {QStringLiteral("shellSettings"), QVariant::fromValue(&shell)},
         {QStringLiteral("windowService"), QVariant::fromValue(&window)}});
    const auto deliverUrls = [&] {
        urlDeliveryReady = true;
        shareResolver.startClipboardMonitoring();
        if (m_urlHandler)
            m_urlHandler->beginDelivery();
        if (!m_openUrl.isEmpty())
            openShared(m_openUrl);
        for (const auto &url : std::as_const(pendingUrls))
            openShared(url);
        pendingUrls.clear();
    };
    const auto startBusiness = [&] {
        plugins.start(m_pluginDirectories);
        pluginStartupReady = true;
        deliverUrls();
        const auto maintenanceError = settings.value("maintenance/lastError").toString();
        if (!maintenanceError.isEmpty()) {
            shellController.toasts()->show(maintenanceError, EchoMusic::ToastService::Warning);
            settings.remove("maintenance/lastError");
        }
        finishSmoke();
        if (!smoke && about.autoCheck())
            QTimer::singleShot(5000, &about, [&] { about.check(true); });
    };
    QObject::connect(&startup, &EchoMusic::StartupController::enteredMain, &app, [&] {
        if (quitting)
            return;
        router.navigateHome();
        search.initialize();
        playback.setStartupHeld(false);
        startBusiness();
    });
    engine.loadFromModule(QStringLiteral("EchoMusic.App"), QStringLiteral("Main"));
    if (!quitting && !useStartup)
        startBusiness();
    if (!engine.rootObjects().isEmpty()) {
        window.attach(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));
        shortcuts.attach(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));
        // Queue real initialization after the startup QML exists. Do not wait
        // for frameSwapped: an occluded/tray launch may never render a frame.
        if (useStartup)
            QTimer::singleShot(0, &startup, &EchoMusic::StartupController::start);
        window.setAppearance(theme.windowFrosted(), theme.dark(), theme.transparency());
        QTimer::singleShot(0, &window, [&] {
            if (!activatedDuringStartup)
                window.applyStartupVisibility();
        });
    }
    const int eventResult = app.exec();
    // Root bindings reference application-owned services (including managers
    // assembled after the engine). Release views while all owners still exist.
    qDeleteAll(engine.rootObjects());
    return exitStatus ? exitStatus : eventResult;
}
