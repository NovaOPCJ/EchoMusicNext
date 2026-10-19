#include "PluginRuntime.h"
#include "../../app/ApplicationShutdown.h"
#include "PluginManager.h"
#include "SharingCommands.h"
#include "PluginDataTransaction.h"
#include "PluginMarketplaceStats.h"
#include "EchoPluginUi.h"
#include "EchoPluginIo.h"
#include "EchoPluginAudio.h"
#include "PlayerSpectrum.h"
#include "FloatingPlayerController.h"
#include <QDataStream>
#include <QWheelEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <cmath>
#include <ctime>
#include <optional>
#include "PlaybackService.h"
#include "AccountService.h"
#include "ShellController.h"
#include "ThemeStore.h"
#include "ShellSettings.h"
#include "NetworkPreferences.h"
#include <QNetworkProxy>
#include <QScopeGuard>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QAbstractItemModelTester>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <ApplicationUrlHandler.h>
#include <QFileOpenEvent>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include "UiTypography.h"
#include "NativeWindow.h"
#include <QQmlComponent>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>
using namespace EchoMusic;
namespace
{
QQuickItem *visual(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *item = visual(child, name))
            return item;
    return nullptr;
}
class DirectoryCapture : public QObject
{
    Q_OBJECT
public:
    QUrl opened;
    ~DirectoryCapture() override
    {
        QDesktopServices::unsetUrlHandler("file");
    }
public slots:
    void open(const QUrl &url)
    {
        opened = url;
    }
};
void write(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
        qFatal("Cannot write fixture");
}
QString package(const QString &root, const QByteArray &entry, QStringList features = {"lifecycle"},
                QStringList capabilities = {})
{
    const auto path = root + "/source";
    QJsonObject manifest{
        {"schemaVersion", 1},
        {"format", "echomod"},
        {"id", "test.runtime"},
        {"name", "Runtime integration"},
        {"version", "1.0.0"},
        {"kind", "qml-js"},
        {"entry", "entry.mjs"},
        {"requires", QJsonObject{{"hostVersion", ">=0.1.0 <1.0.0"},
                                 {"sdkVersion", ">=1.0.0 <2.0.0"},
                                 {"features", QJsonArray::fromStringList(features)}}},
        {"capabilities", QJsonArray::fromStringList(capabilities)},
        {"contexts", QJsonArray{"main"}}};
    write(path + "/manifest.json", QJsonDocument(manifest).toJson());
    write(path + "/entry.mjs", entry);
    return path;
}
struct Host
{
    QTemporaryDir temporary;
    QSettings settings{temporary.path() + "/settings.ini", QSettings::IniFormat};
    ApiClient api{DeviceIdentity::temporary()};
    AccountService account{api};
    PlaybackService playback{api, settings, nullptr, temporary.path() + "/playback.sqlite"};
    ThemeStore theme{settings};
    RouteController router;
    ShellController shell{settings, router};
    QQmlEngine engine;
    PluginRuntime runtime{engine,
                          settings,
                          {playback, theme, shell, router, account, api},
                          temporary.path() + "/plugins"};
    explicit Host(const std::function<void(const QString &)> &prepareRoot = {})
    {
        theme.attachToEngine(engine);
        router.registerPage(
            {"search", "/search", QUrl("qrc:/search.qml"), {}, false, false, false, {}});
        router.navigate("search");
        QObject::connect(&runtime, &PluginRuntime::pluginFailed, &runtime,
                         [](const QString &id, const QString &message) {
                             qWarning().noquote() << id << message;
                         });
        if (prepareRoot)
            prepareRoot(runtime.directory());
        runtime.start();
        QElapsedTimer deadline;
        deadline.start();
        while (runtime.loading() && deadline.elapsed() < 7000)
            QTest::qWait(1);
        if (runtime.loading())
            qFatal("Plugin startup discovery did not settle");
    }
    ~Host()
    {
        QEventLoop loop;
        QObject::connect(&runtime, &PluginRuntime::closed, &loop, &QEventLoop::quit);
        QTimer::singleShot(7000, &loop, &QEventLoop::quit);
        runtime.shutdown();
        loop.exec();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        playback.shutdown();
        api.shutdown();
    }
    QString installation()
    {
        return runtime.data(runtime.index(0), PluginRuntime::InstallationRole).toString();
    }
};
QString pluginPackage(const QString &root, const QString &id, const QByteArray &entry,
                      const QJsonObject &extra = {})
{
    const auto source = package(root + '/' + id, entry);
    QFile file(source + "/manifest.json");
    if (!file.open(QIODevice::ReadOnly))
        qFatal("Cannot read plugin fixture");
    auto manifest = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    manifest["id"] = id;
    for (auto it = extra.begin(); it != extra.end(); ++it)
        manifest[it.key()] = it.value();
    write(source + "/manifest.json", QJsonDocument(manifest).toJson());
    return source;
}
} // namespace
class PluginRuntimeTest : public QObject
{
    Q_OBJECT
private slots:
    void sharedPluginVersionWarnings_data()
    {
        QTest::addColumn<QString>("sharedVersion");
        QTest::addColumn<bool>("compatible");
        QTest::newRow("same") << QString("1.0.0") << true;
        QTest::newRow("source-newer") << QString("0.9.0") << true;
        QTest::newRow("source-older") << QString("2.0.0") << true;
        QTest::newRow("incompatible") << QString("1.0.0") << false;
        QTest::newRow("version-warning-first") << QString("2.0.0") << false;
    }
    void sharedPluginVersionWarnings()
    {
        QFETCH(QString, sharedVersion);
        QFETCH(bool, compatible);
        Host host;
        PluginManager manager(host.runtime, host.settings);
        const auto source = manager.m_sources.first().toMap();
        manager.m_entries = {QVariantMap{{"id", "test.share"},
                                         {"path", "plugins/share"},
                                         {"sourceId", source.value("id")},
                                         {"version", "1.0.0"},
                                         {"compatible", compatible},
                                         {"error", "缺少所需能力"}}};
        manager.m_shareTarget = {{"id", "test.share"},
                                 {"path", "plugins/share"},
                                 {"sourceId", source.value("id")},
                                 {"version", sharedVersion}};
        manager.resolveShared();
        QCOMPARE(manager.shareState(), "ready");
        QVERIFY(manager.shareError().isEmpty());
        QCOMPARE(manager.shareRecord().value("sharedVersion").toString(), sharedVersion);
        const auto expected =
            sharedVersion != "1.0.0"
                ? QString("分享时版本为 v%1，当前插件源提供 v1.0.0。").arg(sharedVersion)
            : compatible ? QString()
                         : QString("缺少所需能力");
        QCOMPARE(manager.shareRecord().value("shareWarning").toString(), expected);
        if (!compatible)
            QVERIFY(!manager.installShared());
    }
    void sharedPluginPagePresentationAndRedirect()
    {
        Host host;
        host.theme.setMotionEnabled(false);
        PluginManager manager(host.runtime, host.settings);
        const auto source = manager.m_sources.first().toMap();
        QImage artwork(64, 64, QImage::Format_RGB32);
        artwork.fill(QColor("#cf4084"));
        const auto iconPath = host.temporary.path() + "/icon.png";
        QVERIFY(artwork.save(iconPath));
        QVariantMap entry{{"id", "test.share"},
                          {"key", "share"},
                          {"path", "plugins/share"},
                          {"sourceId", source.value("id")},
                          {"sourceName", "EchoMusicNext 插件源"},
                          {"sourceUrl", source.value("url")},
                          {"name", "分享插件"},
                          {"description", "展示插件图标与完整描述。这里保留插件源提供的说明，长内容"
                                          "会自然换行，并可随页面滚动。"},
                          {"iconUrl", QUrl::fromLocalFile(iconPath)},
                          {"version", "1.0.0"},
                          {"compatible", true}};
        manager.m_entries = {entry};
        manager.m_shareTarget = {{"id", "test.share"},
                                 {"path", "plugins/share"},
                                 {"sourceId", source.value("id")},
                                 {"sourceUrl", source.value("url")},
                                 {"version", "2.0.0"}};
        manager.resolveShared();
        host.router.registerPage(
            {"plugin-share-resolve", "/plugins/share", QUrl("qrc:/share.qml"), {}, false, false});
        host.router.registerPage(
            {"plugin-management", "/plugins", QUrl("qrc:/plugins.qml"), {"view"}, true, false});
        QVERIFY(host.router.navigate("plugin-share-resolve"));
        ShellSettings settings(host.settings);
        QStringList warnings;
        connect(&host.engine, &QQmlEngine::warnings, &host.engine,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &error : errors)
                        warnings.append(error.toString());
                });
        QQmlComponent component(&host.engine);
        component.setData(R"(
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme
import EchoMusic.Plugins
import EchoMusic.Shell
ApplicationWindow {
 id: win
 required property PluginManager manager
 required property RouteController router
 required property ShellSettings settings
 property real uiScale: settings.uiScale
 width: 1000; height: 900; visible: true; color: Theme.main
 Item {
  width: win.width / win.uiScale; height: win.height / win.uiScale
  scale: win.uiScale; transformOrigin: Item.TopLeft
  Loader {
   anchors.fill: parent
   active: win.router.current !== null && win.router.current.name === "plugin-share-resolve"
   sourceComponent: Component {
    PluginShareResolvePage { manager: win.manager; router: win.router; context: win.router.current; settings: win.settings }
   }
  }
 }
})",
                          QUrl("qrc:/plugin-share-page-test.qml"));
        std::unique_ptr<QObject> root(
            component.createWithInitialProperties({{"manager", QVariant::fromValue(&manager)},
                                                   {"router", QVariant::fromValue(&host.router)},
                                                   {"settings", QVariant::fromValue(&settings)}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        QVERIFY(window && Test::prepareNativeInput(window));
        auto *description = visual(window->contentItem(), "pluginShareDescription");
        QVERIFY(description);
        QCOMPARE(description->property("text").toString(), entry.value("description").toString());
        auto *warning = visual(window->contentItem(), "pluginShareWarningContent");
        QVERIFY(warning);
        QCOMPARE(warning->property("text").toString(),
                 "分享时版本为 v2.0.0，当前插件源提供 v1.0.0。");
        auto *icon = visual(window->contentItem(), "pluginShareIcon");
        QVERIFY(icon);
        QTRY_COMPARE(icon->property("status").toInt(), 1); // Image.Ready
        const auto output = qEnvironmentVariable("ECHO_PLUGIN_SHARE_OUTPUT");
        if (!output.isEmpty())
            QDir().mkpath(output);
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark})
            for (const auto scale : {1.0, 1.25})
                for (const int width : {1000, 480}) {
                    host.theme.setMode(mode);
                    settings.resetZoom();
                    if (scale > 1)
                        for (int step = 0; step < 5; ++step)
                            settings.zoomIn();
                    QCOMPARE(settings.uiScale(), scale);
                    window->setWidth(width);
                    QTest::qWait(100);
                    const auto image = window->grabWindow();
                    QVERIFY(!image.isNull());
                    // Prove the real masked media rendered, not just Image.Ready.
                    const auto point = icon->mapToScene({28, 28}) * image.devicePixelRatio();
                    // The offscreen software backend omits MultiEffect media;
                    // native runs must verify these real pixels for acceptance.
                    if (QGuiApplication::platformName() != "offscreen")
                        QCOMPARE(image.pixelColor(qRound(point.x()), qRound(point.y())),
                                 QColor("#cf4084"));
                    auto *panel = visual(window->contentItem(), "pluginSharePanel");
                    QVERIFY(panel && panel->width() > 0);
                    QVERIFY(panel->mapToScene({0, 0}).x() >= 0);
                    if (!output.isEmpty())
                        QVERIFY(image.save(
                            output +
                            QString("/share-%1-%2-%3.png").arg(int(mode)).arg(scale).arg(width)));
                    if (width == 480) {
                        auto *viewport = visual(window->contentItem(), "pluginShareViewport");
                        QVERIFY(viewport);
                        viewport->setProperty("contentY",
                                              viewport->property("contentHeight").toReal() -
                                                  viewport->height());
                        QTest::qWait(50);
                        auto *action = visual(window->contentItem(), "pluginShareInstall");
                        QVERIFY(action);
                        const auto bottom = action->mapToScene({0, action->height()});
                        QVERIFY(bottom.y() <= window->height());
                        if (!output.isEmpty())
                            QVERIFY(window->grabWindow().save(
                                output +
                                QString("/share-bottom-%1-%2.png").arg(int(mode)).arg(scale)));
                        viewport->setProperty("contentY", 0);
                    }
                }
        entry["iconUrl"] = "";
        entry["description"] = "";
        entry["name"] = " alpha";
        manager.m_entries = {entry};
        manager.resolveShared();
        QCOMPARE(description->property("text").toString(), "暂无描述");
        auto *fallback = visual(window->contentItem(), "pluginShareIconFallback");
        QVERIFY(fallback && fallback->isVisible());
        QCOMPARE(fallback->property("text").toString(), "A");
        entry["name"] = "";
        entry["compatible"] = false;
        entry["error"] = "缺少所需能力";
        manager.m_shareTarget["version"] = "1.0.0";
        manager.m_entries = {entry};
        manager.resolveShared();
        QCOMPARE(fallback->property("text").toString(), "E");
        auto *install = visual(window->contentItem(), "pluginShareInstall");
        QVERIFY(install && !install->isEnabled());
        QCOMPARE(warning->property("text").toString(), "缺少所需能力");
        // A real installed fixture determines the installed branch, including
        // its source-owned version/update decision.
        const auto packagePath =
            pluginPackage(host.temporary.path(), "test.share", "export function activate() {} ");
        QVERIFY(host.runtime.installDirectory(
            packagePath, {{"kind", "marketplace"}, {"id", source.value("id")}}));
        QTRY_VERIFY(!host.runtime.installationForId("test.share").isEmpty());
        entry["compatible"] = true;
        entry["error"] = "";
        manager.m_entries = {entry};
        manager.resolveShared();
        QCOMPARE(manager.shareState(), "installed");
        auto *timer = window->findChild<QObject *>("pluginShareInstalledRedirect");
        QVERIFY(timer);
        QTRY_VERIFY(timer->property("running").toBool());
        window->hide();
        QTRY_VERIFY(!timer->property("running").toBool());
        QTest::qWait(750);
        QCOMPARE(host.router.current()->name(), "plugin-share-resolve");
        window->show();
        QTRY_VERIFY(timer->property("running").toBool());
        // A new share cancels an installed redirect even if the old wait is
        // nearly complete. Catalog resolution restarts only for its new result.
        QTest::qWait(400);
        emit manager.shareOpened();
        manager.m_shareTarget["version"] = "9.0.0";
        manager.m_shareTarget["id"] = "test.missing";
        manager.resolveShared();
        QTest::qWait(400);
        QCOMPARE(host.router.current()->name(), "plugin-share-resolve");
        manager.m_shareTarget["id"] = "test.share";
        manager.resolveShared();
        QTRY_VERIFY(timer->property("running").toBool());
        QPointer<QObject> oldPage = visual(window->contentItem(), "pluginShareResolvePage");
        QVERIFY(host.router.navigate("search"));
        QTRY_VERIFY(!oldPage);
        QTest::qWait(750);
        QCOMPARE(host.router.current()->name(), "search");
        QVERIFY(host.router.navigate("plugin-share-resolve"));
        QTest::qWait(300);
        QCOMPARE(host.router.current()->name(), "plugin-share-resolve");
        QTRY_COMPARE(host.router.current()->name(), "plugin-management");
        QCOMPARE(host.router.current()->query().value("view").toString(), "marketplace");
        QCOMPARE(manager.search(), "test.share");
        QCOMPARE(manager.sourceFilter(), source.value("id").toString());
        host.router.back();
        QCOMPARE(host.router.current()->name(), "search"); // replace removes intermediate page
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void catalogInstallationSourceIsNotCompatibility_data()
    {
        QTest::addColumn<QString>("originKind");
        QTest::addColumn<bool>("matchingSource");
        QTest::newRow("local") << QString("local") << false;
        QTest::newRow("other-marketplace") << QString("marketplace") << false;
        QTest::newRow("same-marketplace") << QString("marketplace") << true;
    }
    void catalogInstallationSourceIsNotCompatibility()
    {
        QFETCH(QString, originKind);
        QFETCH(bool, matchingSource);
        Host host;
        host.theme.setMotionEnabled(false);
        PluginManager manager(host.runtime, host.settings);
        const auto officialSource =
            manager.sources()->data(manager.sources()->index(0), Qt::UserRole + 1).toMap();
        const auto sourceId = officialSource.value("id").toString();
        const QVariantMap origin{{"kind", originKind},
                                 {"id", matchingSource ? sourceId : "other-source"},
                                 {"name", "Installed source"}};
        const auto source = pluginPackage(
            host.temporary.path(), "test.catalog-origin",
            "import {awaitOperation} from 'echo:plugin-sdk'; export function activate(c) { "
            "return awaitOperation(c.io.writeValue('kept',41)); }");
        QVERIFY(host.runtime.installDirectory(source, origin));
        QTRY_VERIFY(!host.runtime.installationForId("test.catalog-origin").isEmpty());
        const auto installation = host.runtime.installationForId("test.catalog-origin");
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Active);
        const auto generation = host.runtime.context(installation)->activationGeneration();
        QVariantMap entry{{"key", "catalog-origin"},
                          {"id", "test.catalog-origin"},
                          {"name", "频谱可视化"},
                          {"version", "2.0.0"},
                          {"compatible", true},
                          {"sourceId", sourceId},
                          {"sourceUrl", officialSource.value("url")},
                          {"sourceName", officialSource.value("name")},
                          {"path", "plugins/catalog-origin"}};
        manager.m_entries = {entry};
        manager.m_loaded = true;
        manager.rebuild();
        auto record =
            manager.catalog()->data(manager.catalog()->index(0), Qt::UserRole + 1).toMap();
        QVERIFY(record.value("compatible").toBool());
        QVERIFY(record.value("installed").toBool());
        QVERIFY(record.value("error").toString().isEmpty());
        QCOMPARE(record.value("updateAvailable").toBool(), matchingSource);
        QCOMPARE(record.value("installLabel").toString(),
                 matchingSource ? QString("更新") : QString("已安装"));
        QCOMPARE(manager.updateCount(), matchingSource ? 1 : 0);
        manager.m_shareTarget = {{"sourceId", sourceId},
                                 {"id", entry.value("id")},
                                 {"path", entry.value("path")},
                                 {"version", "2.0.0"}};
        manager.resolveShared();
        QCOMPARE(manager.shareState(), matchingSource ? QString("ready") : QString("installed"));
        QCOMPARE(manager.shareRecord().value("updateAvailable").toBool(), matchingSource);
        if (!matchingSource) {
            QSignalSpy notices(&manager, &PluginManager::notice);
            QVERIFY(!manager.install("catalog-origin"));
            QVERIFY(!notices.isEmpty());
            QVERIFY(!notices.last()[1].toBool());
            QVERIFY(notices.last()[2].toBool());
            manager.updateAll();
            QVERIFY(!manager.busy());
            QVERIFY(manager.m_updateQueue.isEmpty());
            QCOMPARE(host.runtime.context(installation)->activationGeneration(), generation);
            QCOMPARE(host.runtime.record(installation).value("origin").toMap(), origin);
            auto *kept = host.runtime.context(installation)->io()->readValue("kept");
            const auto guard = host.engine.newQObject(kept);
            QTRY_VERIFY(kept->finished());
            QCOMPARE(kept->result().toInt(), 41);
        }
        if (originKind != "local")
            return;
        host.router.registerPage({"plugin-management",
                                  "/main/settings/plugins",
                                  QUrl("qrc:/qt/qml/EchoMusic/Plugins/PluginManagementPage.qml"),
                                  {"view"},
                                  true,
                                  false,
                                  false,
                                  {}});
        QVERIFY(host.router.navigate("plugin-management", {{"view", "marketplace"}}));
        ShellSettings settings(host.settings);
        QStringList warnings;
        connect(&host.engine, &QQmlEngine::warnings, &host.engine,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &error : errors)
                        warnings.append(error.toString());
                });
        QQmlComponent component(&host.engine);
        component.setData(R"(
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme
import EchoMusic.Plugins
import EchoMusic.Shell
ApplicationWindow {
 id: win
 required property PluginManager manager
 required property RouteController router
 required property ShellSettings settings
 property real uiScale: 1
 width: 500; height: 900; visible: true; color: Theme.main
 Item {
  width: win.width / win.uiScale; height: win.height / win.uiScale
  scale: win.uiScale; transformOrigin: Item.TopLeft
  PluginManagementPage { anchors.fill: parent; manager: win.manager; router: win.router; context: win.router.current; settings: win.settings }
 }
})",
                          QUrl("qrc:/plugin-origin-test.qml"));
        std::unique_ptr<QObject> root(
            component.createWithInitialProperties({{"manager", QVariant::fromValue(&manager)},
                                                   {"router", QVariant::fromValue(&host.router)},
                                                   {"settings", QVariant::fromValue(&settings)}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        QVERIFY(window && Test::prepareNativeInput(window));
        const auto output = qEnvironmentVariable("ECHO_PLUGIN_ORIGIN_OUTPUT");
        if (!output.isEmpty())
            QDir().mkpath(output);
        const QStringList messages{
            "", "缺少 audio.spectrum 能力",
            "缺少所需的 audio.spectrum "
            "能力，请使用提供该能力的主程序版本后再启用插件。当前界面设置与插件私有数据仍保留。"};
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark})
            for (const auto scale : {1.0, 1.25})
                for (int scenario = 0; scenario < messages.size(); ++scenario) {
                    host.theme.setMode(mode);
                    window->setProperty("uiScale", scale);
                    entry["compatible"] = scenario == 0;
                    entry["error"] = messages[scenario];
                    manager.m_entries = {entry};
                    manager.rebuild();
                    QTRY_VERIFY(visual(window->contentItem(), "pluginCard-test.catalog-origin"));
                    auto *card = visual(window->contentItem(), "pluginCard-test.catalog-origin");
                    auto *button = visual(card, "pluginMarketplaceInstall");
                    QVERIFY(button);
                    QCOMPARE(button->property("text").toString(),
                             scenario == 0 ? QString("已安装") : QString("不兼容"));
                    auto *buttonLabel = visual(button, "buttonLabel");
                    QVERIFY(buttonLabel);
                    QTRY_VERIFY(!buttonLabel->property("truncated").toBool());
                    auto *warning = visual(card, "pluginCardWarning");
                    QVERIFY(warning);
                    QCOMPARE(warning->isVisible(), scenario != 0);
                    QTest::qWait(100);
                    if (scenario != 0) {
                        auto *content = visual(warning, "pluginCardWarningContent");
                        auto *icon = visual(content, "buttonGlyph");
                        auto *label = visual(content, "buttonLabel");
                        QVERIFY(icon && label);
                        QTRY_COMPARE(icon->property("status").toInt(), 1);
                        auto *text = qobject_cast<QQuickItem *>(
                            label->property("textItem").value<QObject *>());
                        QVERIFY(text);
                        QCOMPARE(text->property("text").toString(), messages[scenario]);
                        if (scenario == 2)
                            QVERIFY(text->property("lineCount").toInt() > 1);
                        const auto image = window->grabWindow();
                        const qreal dpr = qreal(image.width()) / window->width();
                        const auto pixel =
                            warning->mapToScene({warning->width() - 4, warning->height() / 2}) *
                            dpr;
                        const auto background =
                            image.pixelColor(qRound(pixel.x()), qRound(pixel.y()));
                        const auto foreground = content->property("color").value<QColor>();
                        const qreal opacity = card->opacity();
                        const auto paint = QColor::fromRgbF(
                            foreground.redF() * opacity + background.redF() * (1 - opacity),
                            foreground.greenF() * opacity + background.greenF() * (1 - opacity),
                            foreground.blueF() * opacity + background.blueF() * (1 - opacity));
                        const auto iconArea = icon->mapRectToScene(icon->boundingRect());
                        const auto lineArea = text->mapRectToScene(
                            {0, 0, text->width(), label->property("lineHeight").toReal()});
                        // Measure coverage relative to this translucent card's background.
                        // A thin software-rasterized SVG can have no nearly opaque pixels
                        // along its bottom stroke, unlike a native 2x capture. Include
                        // antialiasing at >=20% coverage for both the icon and text.
                        const auto inkBounds = [&](const QRectF &area) -> std::optional<QRect> {
                            const QPointF origin = area.topLeft() * dpr;
                            const auto pixels = QRectF(origin, area.size() * dpr)
                                                    .toAlignedRect()
                                                    .intersected(image.rect());
                            const double red = paint.red() - background.red();
                            const double green = paint.green() - background.green();
                            const double blue = paint.blue() - background.blue();
                            const double norm = red * red + green * green + blue * blue;
                            QRect bounds;
                            for (int y = pixels.top(); y <= pixels.bottom(); ++y)
                                for (int x = pixels.left(); x <= pixels.right(); ++x) {
                                    const auto color = image.pixelColor(x, y);
                                    const double coverage =
                                        ((color.red() - background.red()) * red +
                                         (color.green() - background.green()) * green +
                                         (color.blue() - background.blue()) * blue) /
                                        norm;
                                    if (coverage >= .2)
                                        bounds = bounds.united(QRect(x, y, 1, 1));
                                }
                            return bounds.isEmpty() ? std::nullopt : std::optional<QRect>(bounds);
                        };
                        const auto iconInk = inkBounds(iconArea);
                        const auto textInk = inkBounds(lineArea);
                        QVERIFY(iconInk && textInk);
                        const qreal delta = qAbs((iconInk->top() + iconInk->bottom()) / 2.0 -
                                                 (textInk->top() + textInk->bottom()) / 2.0);
                        qInfo() << "Warning ink" << int(mode) << scale << scenario << *iconInk
                                << *textInk << "icon box center"
                                << icon->mapToScene({0, icon->height() / 2}).y() * dpr
                                << "text metric center"
                                << label->mapToScene(
                                            {0, label->property("firstLineInkCenter").toReal()})
                                           .y() *
                                       dpr;
                        if (!output.isEmpty())
                            image.save(output + QString("/warning-probe-%1-%2-%3.png")
                                                    .arg(int(mode))
                                                    .arg(scale)
                                                    .arg(scenario));
                        QVERIFY2(
                            delta <= 1,
                            qPrintable(QString("first-line rendered ink differs %1 physical px")
                                           .arg(delta)));
                    }
                    if (!output.isEmpty())
                        QVERIFY(window->grabWindow().save(output + QString("/catalog-%1-%2-%3.png")
                                                                       .arg(int(mode))
                                                                       .arg(scale)
                                                                       .arg(scenario)));
                }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void initTestCase()
    {
        configureUiTypography();
    }

    void dependencyGraphAndVersionFailuresNeverExecuteCode()
    {
        Host host;
        const auto entry = "throw new Error('dependency preflight must not execute this module');";
        const auto missing =
            pluginPackage(host.temporary.path(), "test.missing", entry,
                          {{"dependencies", QJsonObject{{"test.absent", ">=1.0.0 <2.0.0"}}}});
        const auto first =
            pluginPackage(host.temporary.path(), "test.cycle-a", entry,
                          {{"dependencies", QJsonObject{{"test.cycle-b", "1.0.0"}}}});
        const auto second =
            pluginPackage(host.temporary.path(), "test.cycle-b", entry,
                          {{"dependencies", QJsonObject{{"test.cycle-a", "1.0.0"}}}});
        const auto wrong =
            pluginPackage(host.temporary.path(), "test.wrong", entry,
                          {{"dependencies", QJsonObject{{"test.missing", ">=2.0.0"}}}});
        const auto conflictA =
            pluginPackage(host.temporary.path(), "test.conflict-a", entry,
                          {{"conflicts", QJsonObject{{"test.conflict-b", ">=1.0.0"}}}});
        const auto conflictB = pluginPackage(host.temporary.path(), "test.conflict-b", entry);
        QSignalSpy errors(&host.runtime, &PluginRuntime::pluginFailed);
        for (const auto &path : {missing, first, second, wrong, conflictA, conflictB})
            host.runtime.importDirectory(path);
        QTRY_VERIFY(!host.runtime.loading());
        for (const auto &id : {"test.missing", "test.cycle-a", "test.cycle-b", "test.wrong",
                               "test.conflict-a", "test.conflict-b"}) {
            const auto installation = host.runtime.installationForId(id);
            QCOMPARE(host.runtime.state(installation), PluginRuntime::Blocked);
            QVERIFY(!host.runtime.context(installation));
            QVERIFY(!host.runtime.record(installation).value("error").toString().isEmpty());
        }
        QCOMPARE(errors.size(), 0);
        const auto missingId = host.runtime.installationForId("test.missing");
        QVERIFY(host.runtime.setEnabled(missingId, false));
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(host.runtime.state(missingId), PluginRuntime::Disabled);
        QVERIFY(!host.runtime.record(missingId).value("dependencyBlocked").toBool());
        QVERIFY(host.runtime.setEnabled(missingId, true));
        QTRY_COMPARE(host.runtime.state(missingId), PluginRuntime::Blocked);
        QVERIFY(PluginPackage::versionMatches(">=1.0.0 <2.0.0", "1.2.0"));
        QVERIFY(!PluginPackage::versionMatches("^1.0.0", "1.2.0"));
    }

    void dependentServicesActivationCancellationAndProviderReload()
    {
        Host host;
        const auto provider = pluginPackage(host.temporary.path(), "test.zz-provider", R"JS(
            import {awaitOperation} from 'echo:plugin-sdk';
            export function activate(c) {
                c.ui.registerSettings('Settings.qml');
                c.plugins.registerService('volume','1.0.0',{
                    set:function(args) { return awaitOperation(c.player.setVolume(args.percent)).then(function(){return {percent:c.player.volume};}); },
                    wait:function() { return new Promise(function(){}); }
                });
                return awaitOperation(c.io.writeValue('initialized',true));
            }
        )JS");
        write(provider + "/Settings.qml", R"QML(
            import QtQuick
            import EchoMusic.PluginSdk 1.0
            Item { required property EchoPluginContext plugin }
        )QML");
        const auto consumer =
            pluginPackage(host.temporary.path(), "test.aa-consumer", R"JS(
            import * as sdk from 'echo:plugin-sdk';
            export function activate(c) {
                if (typeof sdk.modService !== 'undefined' || typeof c.mods !== 'undefined') throw new Error('Obsolete Mod API must not be exported');
                if (!c.plugins.isAvailable('test.zz-provider')) throw new Error('Provider must be committed first');
                var service=sdk.pluginService(c,'test.zz-provider','volume','>=1.0.0 <2.0.0');
                c.registerCommand('set',function(args){return service.call('set',args);});
                c.registerCommand('wait',function(){return service.call('wait',{});});
            }
        )JS",
                          {{"dependencies", QJsonObject{{"test.zz-provider", ">=1.0.0 <2.0.0"}}}});
        const auto anotherConsumer = pluginPackage(
            host.temporary.path(), "test.bb-consumer",
            "export function activate(c) { c.registerCommand('providerVersion', function() { "
            "return c.plugins.version('test.zz-provider'); }); }",
            {{"dependencies", QJsonObject{{"test.zz-provider", ">=1.0.0 <2.0.0"}}}});
        // Reverse discovery order must not become activation order.
        host.runtime.importDirectory(consumer);
        host.runtime.importDirectory(provider);
        QTRY_VERIFY(!host.runtime.loading());
        const auto providerId = host.runtime.installationForId("test.zz-provider");
        const auto consumerId = host.runtime.installationForId("test.aa-consumer");
        QCOMPARE(host.runtime.state(providerId), PluginRuntime::Active);
        QCOMPARE(host.runtime.state(consumerId), PluginRuntime::Active);
        auto *providerScope = host.runtime.context(providerId);
        QVERIFY(!providerScope->ui()->settingsComponent().isEmpty());
        QQmlComponent settings(&host.engine, providerScope->ui()->settingsComponent());
        QScopedPointer<QObject> settingsView(
            settings.createWithInitialProperties({{"plugin", QVariant::fromValue(providerScope)}}));
        QVERIFY2(settingsView, qPrintable(settings.errorString()));
        settingsView.reset();
        // Closing the provider's UI does not retire its activation or service.
        QCOMPARE(host.runtime.context(providerId), providerScope);
        const auto providerGeneration = providerScope->activationGeneration();
        host.runtime.importDirectory(anotherConsumer);
        QTRY_VERIFY(!host.runtime.loading());
        const auto anotherConsumerId = host.runtime.installationForId("test.bb-consumer");
        QTRY_COMPARE(host.runtime.state(anotherConsumerId), PluginRuntime::Active);
        QCOMPARE(host.runtime.context(providerId), providerScope);
        QCOMPARE(providerScope->activationGeneration(), providerGeneration);
        QPointer<EchoPluginOperation> version =
            host.runtime.executeCommand(anotherConsumerId, "providerVersion");
        QVERIFY(version);
        QTRY_COMPARE(version->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(version->result().toString(), "1.0.0");
        QPointer<EchoPluginOperation> set =
            host.runtime.executeCommand(consumerId, "set", {{"percent", 73}});
        QVERIFY(set);
        QTRY_COMPARE(set->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(set->result().toMap().value("percent").toInt(), 73);
        QCOMPARE(host.playback.volume(), 73.);
        auto *scope = host.runtime.context(consumerId);
        QVERIFY(scope->metaObject()->indexOfProperty("plugins") >= 0);
        QVERIFY(scope->metaObject()->indexOfProperty("mods") < 0);
        QVERIFY(PluginPackage::supportedFeatures().contains("plugins.dependencies"));
        QVERIFY(PluginPackage::supportedFeatures().contains("plugins.services"));
        QVERIFY(!PluginPackage::supportedFeatures().contains("mods.dependencies"));
        QCOMPARE(
            scope->plugins()->call("undeclared.plugin", "volume", "1.0.0", "set", {})->errorCode(),
            "DependencyUnavailable");
        QCOMPARE(
            scope->plugins()->call("test.zz-provider", "volume", ">=2.0.0", "set", {})->errorCode(),
            "ServiceUnavailable");
        QPointer<EchoPluginOperation> waiting = host.runtime.executeCommand(consumerId, "wait");
        QTest::qWait(20);
        const auto generation = scope->activationGeneration();
        QVERIFY(host.runtime.setEnabled(providerId, false));
        QTRY_VERIFY(!host.runtime.loading());
        QVERIFY(waiting && waiting->state() == EchoPluginOperation::Canceled);
        QVERIFY(!host.runtime.context(consumerId));
        QVERIFY(host.runtime.record(consumerId).value("enabled").toBool());
        QCOMPARE(host.runtime.state(consumerId), PluginRuntime::Blocked);
        QCOMPARE(host.runtime.state(anotherConsumerId), PluginRuntime::Blocked);
        QVERIFY(host.runtime.setEnabled(providerId, true));
        QTRY_VERIFY(host.runtime.context(consumerId));
        QTRY_COMPARE(host.runtime.state(consumerId), PluginRuntime::Active);
        QTRY_COMPARE(host.runtime.state(anotherConsumerId), PluginRuntime::Active);
        QVERIFY(host.runtime.context(consumerId)->activationGeneration() != generation);
        // Updating the provider to an incompatible version must keep live code.
        pluginPackage(host.temporary.path(), "test.zz-provider", "export function activate(c){}",
                      {{"version", "2.0.0"}});
        QVERIFY(!host.runtime.installDirectory(provider));
        QCOMPARE(host.runtime.state(providerId), PluginRuntime::Active);
        // A compatible but failing update rolls back the provider's code/data
        // and reactivates its enabled consumers against the restored service.
        pluginPackage(host.temporary.path(), "test.zz-provider",
                      "export function activate(c){throw new Error('provider update failed');}",
                      {{"version", "1.0.1"}});
        QSignalSpy updated(&host.runtime, &PluginRuntime::managementFinished);
        QVERIFY(host.runtime.installDirectory(provider));
        QTRY_COMPARE(updated.size(), 1);
        QVERIFY(!updated.first()[1].toBool());
        QTRY_COMPARE(host.runtime.state(providerId), PluginRuntime::Active);
        QTRY_COMPARE(host.runtime.state(consumerId), PluginRuntime::Active);
        QCOMPARE(host.runtime.record(providerId).value("version").toString(), "1.0.0");
        QVERIFY(host.runtime.uninstall(providerId, false));
        QTRY_VERIFY(host.runtime.installationForId("test.zz-provider").isEmpty());
        QTRY_COMPARE(host.runtime.state(consumerId), PluginRuntime::Blocked);
    }
    void optionalDependencyAndServiceLeasePreserveOwnership()
    {
        Host host;
        const auto provider = pluginPackage(host.temporary.path(), "test.library", R"JS(
            export function activate(c) {
                var lease = c.plugins.registerService('api','1.0.0',{
                    object:function(){return c.player;},
                    wait:function(){return new Promise(function(){});}
                });
                c.registerCommand('close',function(){lease.dispose();return true;});
            }
        )JS");
        const auto consumer = pluginPackage(
            host.temporary.path(), "test.optional", "export function activate(c){}",
            {{"optionalDependencies", QJsonObject{{"test.library", ">=1.0.0 <2.0.0"}}}});
        host.runtime.importDirectory(consumer);
        QTRY_VERIFY(!host.runtime.loading());
        const auto consumerId = host.runtime.installationForId("test.optional");
        QCOMPARE(host.runtime.state(consumerId), PluginRuntime::Active);
        QVERIFY(!host.runtime.context(consumerId)->plugins()->isAvailable("test.library"));
        host.runtime.importDirectory(provider);
        QTRY_VERIFY(!host.runtime.loading());
        const auto providerId = host.runtime.installationForId("test.library");
        auto *plugins = host.runtime.context(consumerId)->plugins();
        QVERIFY(plugins->isAvailable("test.library"));
        QPointer<EchoPluginOperation> object =
            plugins->call("test.library", "api", "1.0.0", "object");
        QTRY_COMPARE(object->state(), EchoPluginOperation::Failed);
        QCOMPARE(object->errorCode(), "InvalidResult");
        QCOMPARE(plugins
                     ->call("test.library", "api", "1.0.0", "wait",
                            {{"object", QVariant::fromValue(&host.playback)}})
                     ->errorCode(),
                 "InvalidArguments");
        QPointer<EchoPluginOperation> pending =
            plugins->call("test.library", "api", "1.0.0", "wait");
        QTest::qWait(20);
        auto *close = host.runtime.executeCommand(providerId, "close");
        QTRY_COMPARE(close->state(), EchoPluginOperation::Succeeded);
        QTRY_COMPARE(pending->state(), EchoPluginOperation::Canceled);
        QVERIFY(host.runtime.setEnabled(providerId, false));
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(host.runtime.state(consumerId), PluginRuntime::Active);
        QVERIFY(!host.runtime.context(consumerId)->plugins()->isAvailable("test.library"));
    }

    void rootDirectoryDiscoveryUpdatesAndRemovesOwnedInput()
    {
        const QByteArray entry =
            "import {awaitOperation} from 'echo:plugin-sdk'; export function activate(c) { "
            "c.registerCommand('read', function() { return "
            "awaitOperation(c.io.readValue('value')); }); "
            "return awaitOperation(c.io.readValue('value')).then(function(v) { if (!v) return "
            "awaitOperation(c.io.writeValue('value', {version:1})); }); }";
        Host host([&](const QString &root) {
            package(root, entry, {"lifecycle", "storage", "commands"});
        });
        QCOMPARE(host.runtime.rowCount(), 1);
        const auto root = host.runtime.directory();
        const auto source = root + "/source";
        const auto installation = host.installation();
        QCOMPARE(
            host.runtime.record(installation).value("origin").toMap().value("directory").toString(),
            QFileInfo(source).canonicalFilePath());
        QVERIFY(host.runtime.context(installation)
                    ->package()
                    .root.startsWith(QFileInfo(root + "/packages").canonicalFilePath() + '/'));
        QVERIFY(!QDir(root + "/local").exists());
        const auto data = root + "/data/" + installation;
        QVERIFY(QFileInfo(data + "/values.sqlite").isFile());
        QSignalSpy finished(&host.runtime, &PluginRuntime::managementFinished);
        QFile manifestFile(source + "/manifest.json");
        QVERIFY(manifestFile.open(QIODevice::ReadOnly));
        auto manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        manifestFile.close();
        manifest["version"] = "2.0.0";
        write(source + "/manifest.json", QJsonDocument(manifest).toJson());
        write(source + "/entry.mjs",
              "import {awaitOperation} from 'echo:plugin-sdk'; export function activate(c) { "
              "return awaitOperation(c.io.writeValue('value', {version:2})).then(function() { "
              "throw new Error('root update fails'); }); }");
        host.runtime.refresh();
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished[0][1].toBool());
        QCOMPARE(host.installation(), installation);
        QCOMPARE(host.runtime.record(installation).value("version").toString(), QString("1.0.0"));
        QCOMPARE(host.runtime.state(installation), PluginRuntime::Active);
        auto *read = host.runtime.executeCommand(installation, "read");
        QVERIFY(read);
        QTRY_COMPARE(read->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(read->result().toMap().value("version").toInt(), 1);
        QVERIFY(!QDir(root + "/transactions/" + installation).exists());
        QVERIFY(host.runtime.uninstall(installation, false));
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(host.runtime.rowCount(), 0);
        QVERIFY(!QDir(source).exists());
        QVERIFY(QDir(data).exists());
        host.runtime.refresh();
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(host.runtime.rowCount(), 0);
        // A deliberate reinstall keeps the retained SQL identity; a complete
        // uninstall removes both the owned folder and that same private store.
        package(root, entry, {"lifecycle", "storage", "commands"});
        host.runtime.refresh();
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(host.installation(), installation);
        QVERIFY(host.runtime.uninstall(installation, true));
        QTRY_VERIFY(!host.runtime.loading());
        QVERIFY(!QDir(source).exists());
        QVERIFY(!QDir(data).exists());
    }
    void rootDiscoverySkipsStoresAndContinuesPastInvalidInput()
    {
        Host host;
        const auto root = host.runtime.directory();
        const auto source = package(root, "export function activate(c) {}");
        QFile manifestFile(source + "/manifest.json");
        QVERIFY(manifestFile.open(QIODevice::ReadOnly));
        auto manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        manifestFile.close();
        write(root + "/aaa-invalid/manifest.json", "{}");
        for (const auto &reserved : QStringList{"packages", "data", "transactions"}) {
            manifest["id"] = "test." + reserved;
            write(root + '/' + reserved + "/manifest.json", QJsonDocument(manifest).toJson());
            write(root + '/' + reserved + "/entry.mjs",
                  "export function activate(c) { throw 'internal store executed'; }");
            QVERIFY(!host.runtime.installDirectory(root + '/' + reserved));
        }
        manifest["id"] = "test.second";
        write(root + "/second/manifest.json", QJsonDocument(manifest).toJson());
        write(root + "/second/entry.mjs", "export function activate(c) {}");
        QSignalSpy finished(&host.runtime, &PluginRuntime::managementFinished);
        host.runtime.refresh();
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(host.runtime.rowCount(), 2);
        QVERIFY(!host.runtime.installationForId("test.runtime").isEmpty());
        QVERIFY(!host.runtime.installationForId("test.second").isEmpty());
        QCOMPARE(finished.count(), 3);
        QVERIFY(!finished[0][1].toBool());
        QVERIFY(finished[1][1].toBool());
        QVERIFY(finished[2][1].toBool());
        QVERIFY(!QDir(root + "/local").exists());
    }
    void refreshReloadsAllInstalledPluginsAndPreservesDisabledState()
    {
        Host host;
        auto source = package(host.temporary.path() + "/first", "export function activate(c) {}");
        QVERIFY(host.runtime.installDirectory(source));
        QTRY_VERIFY(!host.runtime.loading());
        const auto first = host.runtime.installationForId("test.runtime");
        source = package(host.temporary.path() + "/second", "export function activate(c) {}");
        QFile manifestFile(source + "/manifest.json");
        QVERIFY(manifestFile.open(QIODevice::ReadOnly));
        auto manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        manifestFile.close();
        manifest["id"] = "test.second";
        write(source + "/manifest.json", QJsonDocument(manifest).toJson());
        QVERIFY(host.runtime.installDirectory(source));
        QTRY_VERIFY(!host.runtime.loading());
        const auto second = host.runtime.installationForId("test.second");
        const auto firstGeneration =
            host.runtime.context(first)->activationGeneration().toULongLong();
        const auto secondGeneration =
            host.runtime.context(second)->activationGeneration().toULongLong();
        host.runtime.refresh();
        QTRY_VERIFY(!host.runtime.loading());
        QVERIFY(host.runtime.context(first)->activationGeneration().toULongLong() >
                firstGeneration);
        QVERIFY(host.runtime.context(second)->activationGeneration().toULongLong() >
                secondGeneration);
        QVERIFY(host.runtime.setEnabled(first, false));
        QTRY_VERIFY(!host.runtime.loading());
        host.runtime.refresh();
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(host.runtime.state(first), PluginRuntime::Disabled);
        QVERIFY(!host.runtime.context(first));
        QVERIFY(!host.runtime.record(first).value("enabled").toBool());
        QCOMPARE(host.runtime.state(second), PluginRuntime::Active);
        QVERIFY(QDir(source).exists()); // External development inputs remain user-owned.
    }
    void directoryActionOpensTheDisplayedRoot()
    {
        Host host;
        PluginManager manager(host.runtime, host.settings);
        DirectoryCapture capture;
        QDesktopServices::setUrlHandler("file", &capture, "open");
        QVERIFY(manager.openDirectory());
        QCOMPARE(capture.opened, QUrl::fromLocalFile(host.runtime.directory()));
        QVERIFY(QDir(host.runtime.directory()).exists());
        QVERIFY(!QDir(host.runtime.directory() + "/local").exists());
    }

    void managementUpdateRollsBackPackageAndPrivateData()
    {
        Host host;
        const auto source =
            package(host.temporary.path(),
                    "import {awaitOperation} from 'echo:plugin-sdk'; export function activate(c) { "
                    "c.registerCommand('read', function() { return "
                    "awaitOperation(c.io.readValue('value')); }); return "
                    "awaitOperation(c.io.readValue('value')).then(function(v) { if (!v) return "
                    "awaitOperation(c.io.writeValue('value', {version:1})); }); }",
                    {"lifecycle", "storage", "commands"});
        QSignalSpy finished(&host.runtime, &PluginRuntime::managementFinished);
        QVERIFY(host.runtime.installDirectory(source));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished[0][1].toBool());
        const auto installation = host.installation();
        const auto firstGeneration = host.runtime.context(installation)->activationGeneration();
        QFile file(source + "/manifest.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto manifest = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        manifest["version"] = "2.0.0";
        write(source + "/manifest.json", QJsonDocument(manifest).toJson());
        write(source + "/entry.mjs",
              "import {awaitOperation} from 'echo:plugin-sdk'; export function activate(c) { "
              "return awaitOperation(c.io.writeValue('value', {version:2})).then(function(){ throw "
              "new Error('new version fails'); }); }");
        QVERIFY(host.runtime.installDirectory(source));
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY(!finished[1][1].toBool());
        QVERIFY(finished[1][2].toString().contains("new version fails"));
        QCOMPARE(host.runtime.state(installation), PluginRuntime::Active);
        QCOMPARE(host.runtime.record(installation).value("version").toString(), "1.0.0");
        QVERIFY(host.runtime.context(installation)->activationGeneration() != firstGeneration);
        auto *read = host.runtime.executeCommand(installation, "read");
        QTRY_COMPARE(read->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(read->result().toMap().value("version").toInt(), 1);
        QVERIFY(!host.settings.value("plugins/rollback/" + installation).toBool());
        QVERIFY(!QDir(host.temporary.path() + "/plugins/transactions/" + installation).exists());
        QVERIFY(host.runtime.setSafeMode(true));
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Disabled);
        QVERIFY(host.runtime.record(installation).value("enabled").toBool());
        QVERIFY(host.runtime.setSafeMode(false));
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Active);
        QVERIFY(host.runtime.uninstall(installation, false));
        QTRY_COMPARE(finished.count(), 3);
        QCOMPARE(host.runtime.rowCount(), 0);
        QVERIFY(
            QFileInfo(host.temporary.path() + "/plugins/data/" + installation + "/values.sqlite")
                .isFile());
        // Reinstall retains the private-data identity, and explicit removal clears it.
        manifest["version"] = "1.0.0";
        write(source + "/manifest.json", QJsonDocument(manifest).toJson());
        write(source + "/entry.mjs", "export function activate(c) {}");
        QVERIFY(host.runtime.installDirectory(source));
        QTRY_COMPARE(finished.count(), 4);
        QCOMPARE(host.installation(), installation);
        QVERIFY(host.runtime.uninstall(installation, true));
        QTRY_COMPARE(finished.count(), 5);
        QVERIFY(!QDir(host.temporary.path() + "/plugins/data/" + installation).exists());
        QVERIFY(QDir(source).exists()); // Uninstall never removes an external author workspace.
    }
    void metadataCommitFailureRetiresNewCodeAndRetainsRecoverySnapshot()
    {
        Host host;
        const auto source = package(host.temporary.path(), "export function activate(c) {}");
        QSignalSpy finished(&host.runtime, &PluginRuntime::managementFinished);
        QVERIFY(host.runtime.installDirectory(source));
        QTRY_COMPARE(finished.count(), 1);
        QFile manifestFile(source + "/manifest.json");
        QVERIFY(manifestFile.open(QIODevice::ReadOnly));
        auto manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
        manifestFile.close();
        manifest["version"] = "2.0.0";
        write(source + "/manifest.json", QJsonDocument(manifest).toJson());
        write(source + "/entry.mjs",
              "var done; export function activate(c) { return new Promise(function(resolve) { done "
              "= resolve; }); } export function complete() { done(); }");
        QVERIFY(host.runtime.installDirectory(source));
        const auto installation = host.installation();
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Activating);
        const auto file = host.settings.fileName();
        QVERIFY(QFile::rename(file, file + ".saved"));
        QVERIFY(QDir().mkpath(file));
        QString installedEntry;
        QDir versions(host.runtime.directory() + "/packages/" + installation);
        for (const auto &version : versions.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile file(versions.filePath(version + "/manifest.json"));
            QVERIFY(file.open(QIODevice::ReadOnly));
            if (QJsonDocument::fromJson(file.readAll()).object().value("version") == "2.0.0")
                installedEntry = versions.filePath(version + "/entry.mjs");
        }
        QVERIFY(!installedEntry.isEmpty());
        auto module = host.engine.importModule(installedEntry);
        QVERIFY2(!module.isError(), qPrintable(module.toString()));
        QVERIFY(!module.property("complete").call().isError());
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY(!finished.last()[1].toBool());
        QCOMPARE(host.runtime.record(installation).value("version").toString(), QString("1.0.0"));
        QVERIFY(host.runtime.state(installation) != PluginRuntime::Active);
        QVERIFY(host.settings.value("plugins/rollback/" + installation).toBool());
        QVERIFY(QDir(host.runtime.directory() + "/transactions/" + installation).exists());
        QVERIFY(QDir(file).removeRecursively());
        QVERIFY(QFile::rename(file + ".saved", file));
        host.settings.sync();
    }
    void localBatchContinuesAfterInvalidPackage()
    {
        Host host;
        PluginManager manager(host.runtime, host.settings);
        const auto source = package(host.temporary.path(), "export function activate(c) {}");
        QSignalSpy finished(&host.runtime, &PluginRuntime::managementFinished);
        QSignalSpy notice(&manager, &PluginManager::notice);
        QVERIFY(
            manager.installLocalBatch({QUrl::fromLocalFile(host.temporary.path() + "/invalid.txt"),
                                       QUrl::fromLocalFile(source)}));
        QTRY_VERIFY(!manager.busy());
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        QVERIFY(notice.count() >= 2);
        QVERIFY(notice.last()[1].toBool());
        // A valid manifest with an unsafe link fails during worker capture. The
        // completion identity must still release manager busy after rejection.
        QVERIFY(QFile::link(source + "/entry.mjs", source + "/unsafe.mjs"));
        QVERIFY(manager.installLocal(QUrl::fromLocalFile(source)));
        QTRY_VERIFY(!manager.busy());
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY(!finished.last()[1].toBool());
        QCOMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
    }
    void arrivingInstallFilesCompleteDependencyBatch()
    {
        Host host;
        PluginManager manager(host.runtime, host.settings);
        const auto provider = pluginPackage(host.temporary.path(), "test.batch-provider",
                                            "export function activate(c){}");
        const auto consumer = pluginPackage(
            host.temporary.path(), "test.batch-consumer", "export function activate(c){}",
            {{"dependencies", QJsonObject{{"test.batch-provider", "1.0.0"}}}});
        QSignalSpy finished(&host.runtime, &PluginRuntime::managementFinished);
        QVERIFY(manager.installLocalBatch({QUrl::fromLocalFile(consumer)}));
        QVERIFY(manager.busy());
        QVERIFY(manager.installLocalBatch({QUrl::fromLocalFile(provider)}));
        QTRY_COMPARE(finished.count(), 2);
        QTRY_VERIFY(!manager.busy() && !host.runtime.loading());
        for (const auto &id : {"test.batch-provider", "test.batch-consumer"})
            QCOMPARE(host.runtime.state(host.runtime.installationForId(id)), PluginRuntime::Active);
        QVERIFY(finished.first()[1].toBool());
        QVERIFY(finished.last()[1].toBool());
    }
    void qtFileOpenQueuesBeforeServiceInitialization()
    {
        ApplicationUrlHandler handler;
        const QUrl url("echomusicnext://plugin?source=https://github.com/owner/"
                       "repo&id=org.test.plugin&version=1.0.0&path=plugins/test");
        QSignalSpy delivered(&handler, &ApplicationUrlHandler::urlRequested);
        QFileOpenEvent first(url);
        QCoreApplication::sendEvent(QCoreApplication::instance(), &first);
        const auto queuedPackage = QUrl::fromLocalFile(QDir::tempPath() + "/queued.echomod");
        QFileOpenEvent queued(queuedPackage);
        QCoreApplication::sendEvent(QCoreApplication::instance(), &queued);
        QCOMPARE(handler.takePending(1), QList<QUrl>{url});
        QCOMPARE(delivered.count(), 0);
        handler.beginDelivery();
        QCOMPARE(delivered.count(), 1);
        QCOMPARE(delivered[0][0].toUrl(), queuedPackage);
        QFileOpenEvent next(url);
        QCoreApplication::sendEvent(QCoreApplication::instance(), &next);
        QCOMPARE(delivered.count(), 2);
        const auto local = QUrl::fromLocalFile(QDir::tempPath() + "/native-open.echomod");
        QFileOpenEvent plugin(local);
        QCoreApplication::sendEvent(QCoreApplication::instance(), &plugin);
        QCOMPARE(delivered.count(), 3);
        QCOMPARE(delivered[2][0].toUrl(), local);
        QFileOpenEvent zip(QUrl::fromLocalFile(QDir::tempPath() + "/old.zip"));
        QCoreApplication::sendEvent(QCoreApplication::instance(), &zip);
        QCOMPARE(delivered.count(), 3);
    }
    void githubMirrorFallbackAndPostShareUnifiedProxy()
    {
        Host host;
        NetworkPreferences preferences(host.settings, host.api);
        PluginManager manager(host.runtime, host.settings);
        manager.setNetworkPreferences(&preferences);
        QTcpServer proxy;
        proxy.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        QList<QByteArray> requests;
        bool rejectOrigin = false;
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            auto *peer = proxy.nextPendingConnection();
            connect(peer, &QTcpSocket::readyRead, peer, [&, peer] {
                const auto header = peer->property("header").toByteArray() + peer->readAll();
                peer->setProperty("header", header);
                if (!header.contains("\r\n\r\n") || peer->property("answered").toBool())
                    return;
                peer->setProperty("answered", true);
                requests.append(header.left(header.indexOf("\r\n")));
                const bool rejected = rejectOrigin || requests.last().contains("/mirror/");
                const QByteArray body(rejected ? "rejected" : "actual download bytes");
                peer->write(
                    QByteArray(rejected ? "HTTP/1.1 503 Unavailable\r\n" : "HTTP/1.1 200 OK\r\n") +
                    "Content-Length: " + QByteArray::number(body.size()) +
                    "\r\nConnection: close\r\n\r\n" + body);
                peer->disconnectFromHost();
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        QSignalSpy applied(&preferences, &NetworkPreferences::applied);
        preferences.apply("fixed_servers", {},
                          QString("http://127.0.0.1:%1").arg(proxy.serverPort()), {}, {}, false,
                          "<local>", 0, 60);
        QTRY_COMPARE(applied.size(), 1);
        const auto previous = QNetworkProxy::applicationProxy();
        const auto restore = qScopeGuard([&] { QNetworkProxy::setApplicationProxy(previous); });
        preferences.installApplicationTransport();
        preferences.setGithubProxyUrl("https://mirror.invalid/");
        QList<QUrl> resolved;
        // Keep real mirror selection, retry and Qt proxy I/O; only map the
        // external destination to a private transport fixture (not a catalog).
        manager.m_resolveUrl = [&](const QUrl &url) {
            resolved.append(url);
            return QUrl("http://plugin-source.invalid/" +
                        QString(url.host() == "mirror.invalid" ? "mirror/" : "origin/") +
                        "package.zip");
        };
        int completed = 0;
        QByteArray result;
        QString failure;
        const auto finish = [&](QByteArray bytes, QString error) {
            ++completed;
            result = bytes;
            failure = error;
        };
        const QUrl source("https://github.com/owner/repo/releases/download/v1/package.zip");
        manager.get(source, 1024, finish);
        QTRY_COMPARE(completed, 1);
        QVERIFY(failure.isEmpty());
        QCOMPARE(result, QByteArray("actual download bytes"));
        QCOMPARE(resolved.size(), 2);
        QVERIFY(resolved[0].toString().startsWith("https://mirror.invalid/https://github.com/"));
        QCOMPARE(resolved[1], source);
        QCOMPARE(requests.size(), 2);
        QVERIFY(requests[0].startsWith("GET http://plugin-source.invalid/mirror/"));
        QVERIFY(requests[1].startsWith("GET http://plugin-source.invalid/origin/"));
        // Statistics/other writes must never use the mirror or be replayed.
        rejectOrigin = true;
        manager.get(source, 1024, finish, QByteArray("{}"));
        QTRY_COMPARE(completed, 2);
        QVERIFY(failure.contains("503"));
        QCOMPARE(resolved.size(), 3);
        QCOMPARE(resolved[2], source);
        QCOMPARE(requests.size(), 3);
        QVERIFY(requests[2].startsWith("POST http://plugin-source.invalid/origin/"));
        manager.shutdown();
    }
    void sourceCatalogNetworkCacheAndOfficialProtection()
    {
        Host host;
        PluginManager manager(host.runtime, host.settings);
        QAbstractItemModelTester catalogTester(
            manager.catalog(), QAbstractItemModelTester::FailureReportingMode::QtTest);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        bool fail = false;
        const auto source = package(host.temporary.path(), "export function activate(c) {}");
        QFile file(source + "/manifest.json");
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto manifest = file.readAll();
        file.close();
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                auto request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                if (!request.contains("\r\n\r\n"))
                    return;
                const QByteArray body =
                    fail ? QByteArray("unavailable")
                    : request.startsWith("GET /echo-plugins.json ")
                        ? QByteArray(
                              R"({"schemaVersion":2,"plugins":[{"path":"plugins/test","artifact":"artifacts/test.echomod","sha256":"0000000000000000000000000000000000000000000000000000000000000000"}]})")
                        : manifest;
                socket->write(
                    QByteArray(fail ? "HTTP/1.1 503 Unavailable\r\n" : "HTTP/1.1 200 OK\r\n") +
                    "Content-Length: " + QByteArray::number(body.size()) +
                    "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            });
        });
        manager.m_resolveUrl = [&](const QUrl &url) {
            return QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) +
                        (url.path().endsWith("echo-plugins.json") ? "/echo-plugins.json"
                                                                  : "/manifest.json"));
        };
        manager.refreshCatalog();
        QTRY_VERIFY(!manager.catalogLoading());
        QCOMPARE(manager.catalog()->rowCount(), 1);
        QVERIFY(manager.sourceError().isEmpty());
        manager.setSearch("nothing");
        QCOMPARE(manager.catalog()->rowCount(), 0);
        manager.setSearch("Runtime");
        QCOMPARE(manager.catalog()->rowCount(), 1);
        QVERIFY(!manager.removeSource("github:hoowhoami/echomusicnextplugins"));
        QVERIFY(!manager.addSource("https://github.com/owner/repo/../bad", "bad"));
        fail = true;
        manager.refreshCatalog(true);
        QTRY_VERIFY(!manager.catalogLoading());
        QCOMPARE(manager.catalog()->rowCount(), 1);
        QVERIFY(manager.sourceError().contains("503"));
        QVERIFY(manager.patchSource("github:hoowhoami/echomusicnextplugins", "Official", false));
        QCOMPARE(manager.catalog()->rowCount(), 0);
        QCOMPARE(manager.sourceOptions().size(), 2);
        QVERIFY(manager.sourceOptions()[1].toMap().value("disabled").toBool());
        QUrl shared("echomusicnext://plugin");
        QUrlQuery query;
        query.addQueryItem("source", "https://github.com/owner/unavailable");
        query.addQueryItem("id", "test.runtime");
        query.addQueryItem("version", "1.0.0");
        query.addQueryItem("path", "plugins/test");
        shared.setQuery(query);
        QVERIFY(manager.openShare(shared));
        QCOMPARE(manager.shareState(), "need-source");
        QVERIFY(manager.acceptShareSource());
        QTRY_COMPARE(manager.shareState(), "failed");
        QVERIFY(manager.shareError().contains("503"));
        QCOMPARE(manager.sources()->rowCount(), 1);
        manager.shutdown();
    }
    void settingsContributionRetiresWithScope()
    {
        Host host;
        const auto source =
            package(host.temporary.path(),
                    "export function activate(c) { c.ui.registerSettings('Settings.qml'); }",
                    {"lifecycle", "ui.settings"});
        write(source + "/Settings.qml",
              "import QtQuick\nItem { required property var plugin; implicitHeight: 100 }");
        host.runtime.importDirectory(source);
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        auto *ui = host.runtime.context(host.installation())->ui();
        QVERIFY(!ui->settingsComponent().isEmpty());
        QPointer<EchoPluginUi> retired = ui;
        QVERIFY(host.runtime.setEnabled(host.installation(), false));
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Disabled);
        QTRY_VERIFY(retired.isNull());
    }

    void managementHeaderAndLoadingContentStayAligned()
    {
        Host host;
        host.theme.setMotionEnabled(false);
        PluginManager manager(host.runtime, host.settings);
        ShellSettings settings(host.settings);
        QStringList warnings;
        connect(&host.engine, &QQmlEngine::warnings, &host.engine,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &error : errors)
                        warnings.append(error.toString());
                });
        QQmlComponent component(&host.engine);
        component.setData(R"(
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme
import EchoMusic.Plugins
import EchoMusic.Shell
ApplicationWindow {
    id: win
    required property PluginManager manager
    required property RouteController router
    required property ShellSettings settings
    property real uiScale: 1
    width: 1200; height: 650; visible: true; color: Theme.main
    Item {
        width: win.width / win.uiScale; height: win.height / win.uiScale
        scale: win.uiScale; transformOrigin: Item.TopLeft
        PluginManagementPage {
            anchors.fill: parent; manager: win.manager; router: win.router
            context: win.router.current; settings: win.settings
        }
        // Use the production refresh/update action independently of network completion timing.
        PluginActionButton {
            objectName: "loadingTextProbe"
            x: 24; y: parent.height - height - 24; width: 180
            glyph: "refresh"; text: "更新 Update"
        }
    }
})",
                          QUrl("qrc:/plugin-alignment-test.qml"));
        std::unique_ptr<QObject> root(
            component.createWithInitialProperties({{"manager", QVariant::fromValue(&manager)},
                                                   {"router", QVariant::fromValue(&host.router)},
                                                   {"settings", QVariant::fromValue(&settings)}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        QVERIFY(window && Test::prepareNativeInput(window));
        auto *page = visual(window->contentItem(), "pluginManagementPage");
        auto *refresh = visual(page, "pluginRefresh");
        auto *probe = visual(window->contentItem(), "loadingTextProbe");
        QVERIFY(page && refresh && probe);
        const auto output = qEnvironmentVariable("ECHO_PLUGIN_ALIGNMENT_OUTPUT");
        if (!output.isEmpty())
            QVERIFY(QDir().mkpath(output));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark})
            for (qreal scale : {1.0, 1.25})
                for (int width : {1200, 800, 420}) {
                    host.theme.setMode(mode);
                    window->setProperty("uiScale", scale);
                    window->resize(width, 650);
                    auto *emptyIcon = visual(page, "pluginEmptyIcon");
                    QVERIFY(emptyIcon);
                    QTRY_COMPARE(emptyIcon->size(), QSizeF(48, 48));
                    // The trailing action must share the page's 24px inset;
                    // a stretching nested Flex leaves a large blank tail.
                    QTRY_VERIFY(qAbs(refresh->mapToItem(page, {refresh->width(), 0}).x() -
                                     (page->width() - 24)) < 0.5);
                    for (bool loading : {false, true}) {
                        for (auto *button : {refresh, probe}) {
                            button->setProperty("loading", loading);
                            button->setProperty("enabled", !loading);
                            auto *row = visual(button, "buttonContentRow");
                            QVERIFY(row);
                            QTRY_VERIFY(
                                qAbs(row->mapToItem(button, {row->width() / 2, row->height() / 2})
                                         .x() -
                                     button->width() / 2) < 0.5);
                            if (loading) {
                                auto *spinner = visual(button, "buttonSpinner");
                                QVERIFY(spinner && spinner->isVisible());
                                QTRY_VERIFY(qAbs(spinner
                                                     ->mapToItem(button, {spinner->width() / 2,
                                                                          spinner->height() / 2})
                                                     .y() -
                                                 button->height() / 2) < 0.5);
                            }
                        }
                        if (QGuiApplication::platformName() == "offscreen")
                            continue;
                        auto *spinner = visual(refresh, "buttonSpinner");
                        if (loading)
                            QTRY_COMPARE(spinner->property("status").toInt(), 1);
                        QSignalSpy frames(window, &QQuickWindow::frameSwapped);
                        window->update();
                        QTRY_VERIFY(!frames.isEmpty());
                        const auto image = window->grabWindow();
                        QVERIFY(!image.isNull());
                        if (loading) {
                            const auto dpr = window->devicePixelRatio();
                            const auto topLeft = refresh->mapToScene({0, 0}) * dpr;
                            const qreal unit = scale * dpr;
                            const auto fill = image.pixelColor(qRound(topLeft.x() + 5 * unit),
                                                               qRound(topLeft.y() + 5 * unit));
                            QRect ink;
                            for (int y = qCeil(topLeft.y() + 4 * unit);
                                 y < qFloor(topLeft.y() + (refresh->height() - 4) * unit); ++y)
                                for (int x = qCeil(topLeft.x() + 4 * unit);
                                     x < qFloor(topLeft.x() + (refresh->width() - 4) * unit); ++x) {
                                    const auto pixel = image.pixelColor(x, y);
                                    if (qAbs(qGray(pixel.rgb()) - qGray(fill.rgb())) > 12)
                                        ink = ink.united(QRect(x, y, 1, 1));
                                }
                            QVERIFY(!ink.isEmpty());
                            const QPointF inkCenter(ink.x() + ink.width() / 2.0,
                                                    ink.y() + ink.height() / 2.0);
                            const auto center =
                                refresh->mapToScene({refresh->width() / 2, refresh->height() / 2}) *
                                dpr;
                            QVERIFY2(QLineF(inkCenter, center).length() <= 1.5 * unit,
                                     "Rendered loading ring must be centered in the icon action");
                        }
                        if (!output.isEmpty())
                            QVERIFY(image.save(output + QString("/header-%1-%2-%3-%4.png")
                                                            .arg(int(mode))
                                                            .arg(scale)
                                                            .arg(width)
                                                            .arg(loading)));
                    }
                }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }

    void managementPageUsesProductionModelsAndDialogs()
    {
        Host host;
        PluginManager manager(host.runtime, host.settings);
        ShellSettings settings(host.settings);
        const auto source =
            package(host.temporary.path(),
                    "export function activate(c) { c.ui.registerSettings('Settings.qml'); "
                    "c.registerCommand('fail', function() { throw new Error('actual callback "
                    "failure'); }); }",
                    {"lifecycle", "ui.settings", "commands"});
        write(source + "/Settings.qml",
              "import QtQuick\nimport EchoMusic.PluginSdk\nItem { required property "
              "EchoPluginContext plugin; implicitHeight: 80; objectName: 'actualPluginSettings' }");
        QSignalSpy installed(&host.runtime, &PluginRuntime::managementFinished);
        QVERIFY(host.runtime.installDirectory(source));
        QTRY_COMPARE(installed.count(), 1);
        host.router.registerPage({"plugin-management",
                                  "/main/settings/plugins",
                                  QUrl("qrc:/qt/qml/EchoMusic/Plugins/PluginManagementPage.qml"),
                                  {"view"},
                                  true,
                                  false,
                                  false,
                                  {}});
        QVERIFY(host.router.navigate("plugin-management"));
        QStringList warnings;
        connect(&host.engine, &QQmlEngine::warnings, &host.engine,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &error : errors)
                        warnings.append(error.toString());
                });
        QQmlComponent component(&host.engine);
        component.setData(R"(
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme
import EchoMusic.Plugins
import EchoMusic.Shell
ApplicationWindow {
 id: win
 required property PluginManager manager
 required property RouteController router
 required property ShellSettings settings
 property real uiScale: 1
 width:900; height:650; visible:true; color:Theme.main
 Item {
  width: win.width / win.uiScale; height: win.height / win.uiScale; scale: win.uiScale; transformOrigin: Item.TopLeft
  PluginManagementPage { anchors.fill:parent; manager:win.manager; router:win.router; context:win.router.current; settings:win.settings }
 }
})",
                          QUrl("qrc:/management-test.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> root(
            component.createWithInitialProperties({{"manager", QVariant::fromValue(&manager)},
                                                   {"router", QVariant::fromValue(&host.router)},
                                                   {"settings", QVariant::fromValue(&settings)}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY(!visual(window->contentItem(), "pluginRecoveryBanner"));
        QVERIFY(!visual(window->contentItem(), "pluginResumeLoading"));
        const auto exitArtifacts = qEnvironmentVariable("ECHO_PLUGIN_EXIT_ARTIFACTS");
        if (!exitArtifacts.isEmpty()) {
            QDir().mkpath(exitArtifacts);
            window->resize(1000, 800);
            for (const auto mode : {ThemeStore::Dark, ThemeStore::Light}) {
                host.theme.setMode(mode);
                for (const auto scale : {1.0, 1.25}) {
                    window->setProperty("uiScale", scale);
                    QTest::mouseMove(window, {window->width() - 4, window->height() - 4});
                    QTest::qWait(100);
                    const auto name = QString("/plugins-%1-%2").arg(int(mode)).arg(scale);
                    QVERIFY(window->grabWindow().save(exitArtifacts + name + ".png"));
                    auto *surface = visual(window->contentItem(), "pluginCard-test.runtime");
                    QVERIFY(surface);
                    QTest::mouseMove(window,
                                     surface->mapToScene({surface->width() - 8, 8}).toPoint());
                    QTest::qWait(220);
                    QVERIFY(window->grabWindow().save(exitArtifacts + name + "-hover.png"));
                }
            }
            window->setProperty("uiScale", 1.0);
            host.theme.setMode(ThemeStore::Light);
            host.theme.selectSolid(QColor("#fff0ed"));
            QTest::mouseMove(window, {window->width() - 4, window->height() - 4});
            QTest::qWait(100);
            QVERIFY(window->grabWindow().save(exitArtifacts + "/plugins-tinted.png"));
            host.theme.selectEcho();
            window->resize(900, 650);
            QTest::mouseMove(window, {window->width() - 4, window->height() - 4});
        }
        QVERIFY(!root->findChild<QObject *>("pluginPackageFileDialog"));
        auto *installPackage = visual(window->contentItem(), "pluginInstallPackage");
        QVERIFY(installPackage);
        QCOMPARE(installPackage->property("text").toString(), QString("安装插件"));
        QVERIFY(QMetaObject::invokeMethod(installPackage, "clicked"));
        QPointer<QObject> fileDialog = root->findChild<QObject *>("pluginPackageFileDialog");
        QVERIFY(fileDialog);
        QCOMPARE(fileDialog->property("title").toString(), QString("安装 EchoMusicNext 插件"));
        QCOMPARE(fileDialog->property("nameFilters").toStringList(),
                 QStringList{QString("EchoMusicNext 插件 (*.echomod)")});
        QTRY_VERIFY(fileDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(fileDialog, "close"));
        QTRY_VERIFY(!fileDialog);
        // The native picker must remain usable after complete destruction and
        // be released on route retirement as well as explicit close.
        QVERIFY(QMetaObject::invokeMethod(installPackage, "clicked"));
        fileDialog = root->findChild<QObject *>("pluginPackageFileDialog");
        QVERIFY(fileDialog);
        QVERIFY(host.router.navigate("search"));
        QTRY_VERIFY(!fileDialog);
        QVERIFY(host.router.navigate("plugin-management"));
        QTRY_VERIFY(visual(window->contentItem(), "pluginCard-test.runtime"));
        auto *card =
            qobject_cast<QQuickItem *>(visual(window->contentItem(), "pluginCard-test.runtime"));
        QTRY_VERIFY(card->height() > 100);
        auto *idLabel = visual(card, "pluginCardId");
        auto *sourceLabel = visual(card, "pluginCardSource");
        QVERIFY(idLabel && sourceLabel);
        QVERIFY(idLabel->mapToScene({0, 0}).x() < sourceLabel->mapToScene({0, 0}).x());
        QVERIFY(visual(card, "pluginCardDivider"));
        auto *statusTag = visual(card, "pluginCardStatus");
        QVERIFY(statusTag);
        QVERIFY(statusTag->x() > card->width() * 0.5);
        auto *headerIcon = visual(window->contentItem(), "pluginHeaderIcon");
        QVERIFY(headerIcon);
        auto *directoryLabel = visual(window->contentItem(), "pluginDirectory");
        QVERIFY(directoryLabel);
        QCOMPARE(directoryLabel->property("text").toString(), host.runtime.directory());
        QVERIFY(QFile::exists(":/qt/qml/EchoMusic/Controls/icons/puzzle.svg"));
        auto *headerImage = visual(headerIcon, "iconImage");
        QVERIFY(headerImage);
        QVERIFY2(!headerImage->property("source").toUrl().isEmpty(),
                 qPrintable(QString("icon visible=%1 color=%2 geometry=%3x%4")
                                .arg(headerIcon->isVisible())
                                .arg(headerIcon->property("color").toString())
                                .arg(headerIcon->width())
                                .arg(headerIcon->height())));
        QTRY_COMPARE(headerIcon->property("status").toInt(), 1);
        QVERIFY(headerIcon->width() > 0);
        QVERIFY(headerIcon->height() > 0);
        QVERIFY(QMetaObject::invokeMethod(card, "failureRequested",
                                          Q_ARG(QString, host.installation())));
        QPointer<QObject> failureDialog = root->findChild<QObject *>("pluginFailureDialog");
        QVERIFY(failureDialog);
        QTRY_VERIFY(failureDialog->property("visible").toBool());
        auto *failed = host.runtime.executeCommand(host.installation(), "fail");
        QTRY_COMPARE(failed->state(), EchoPluginOperation::Failed);
        QVERIFY(!host.runtime.safeMode());
        QTRY_VERIFY(visual(window->contentItem(), "pluginFailureText"));
        auto *failureText = visual(window->contentItem(), "pluginFailureText");
        QTRY_VERIFY(failureText->property("text").toString().contains("actual callback failure"));
        auto *clearFailure = visual(window->contentItem(), "pluginClearFailure");
        QVERIFY(clearFailure);
        QVERIFY(QMetaObject::invokeMethod(clearFailure, "clicked"));
        QTRY_VERIFY(!failureDialog);
        QVERIFY(host.runtime.record(host.installation()).value("failure").toMap().isEmpty());
        // The bounded row Repeater replaces records after dataChanged. Reacquire
        // its visual delegate after the callback/clear sequence.
        QTRY_VERIFY(visual(window->contentItem(), "pluginCard-test.runtime"));
        card = visual(window->contentItem(), "pluginCard-test.runtime");
        QVERIFY(QMetaObject::invokeMethod(card, "settingsRequested",
                                          Q_ARG(QString, host.installation())));
        QTRY_VERIFY(visual(window->contentItem(), "actualPluginSettings"));
        QPointer<QObject> dialog = root->findChild<QObject *>("pluginSettingsDialog");
        QVERIFY(dialog);
        QVERIFY(dialog->property("visible").toBool());
        QVERIFY(host.router.navigate("search"));
        QTRY_VERIFY(!dialog);
        QVERIFY(host.router.navigate("plugin-management"));
        auto *safe = visual(window->contentItem(), "pluginSafeMode");
        QVERIFY(safe);
        QVERIFY(safe->setProperty("checked", true));
        QVERIFY(QMetaObject::invokeMethod(safe, "toggled"));
        QTRY_VERIFY(host.runtime.safeMode());
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Disabled);
        // Persisted preference is authoritative; QML consumes its notification.
        QVERIFY(host.settings.value("plugins/safeMode").toBool());
        const auto output = qEnvironmentVariable("ECHO_PLUGIN_UI_OUTPUT");
        if (!output.isEmpty()) {
            QDir().mkpath(output);
            host.theme.setMode(ThemeStore::Light);
            QTest::qWait(150);
            QVERIFY(window->grabWindow().save(output + "/installed-light.png"));
            host.theme.setMode(ThemeStore::Dark);
            QTest::qWait(150);
            QVERIFY(window->grabWindow().save(output + "/installed-dark.png"));
        }
        card = visual(window->contentItem(), "pluginCard-test.runtime");
        QVERIFY(card);
        QVERIFY(QMetaObject::invokeMethod(card, "uninstallRequested",
                                          Q_ARG(QString, host.installation())));
        QTRY_VERIFY(visual(window->contentItem(), "pluginRemoveDataLabel"));
        QPointer<QObject> uninstallDialog = root->findChild<QObject *>("pluginUninstallDialog");
        QVERIFY(uninstallDialog);
        QTRY_COMPARE(uninstallDialog->property("opacity").toReal(), 1.0);
        if (!output.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(window->grabWindow().save(output + "/uninstall-dialog.png"));
        }
        QVERIFY(QMetaObject::invokeMethod(uninstallDialog, "close"));
        QTRY_VERIFY(!uninstallDialog);
        QVERIFY(!root->findChild<QObject *>("pluginLocalInstallOverlay"));
        QMimeData draggedPackage;
        draggedPackage.setUrls({QUrl::fromLocalFile(source)});
        QDragEnterEvent enter(QPoint(300, 300), Qt::CopyAction, &draggedPackage, Qt::LeftButton,
                              Qt::NoModifier);
        QCoreApplication::sendEvent(window, &enter);
        QVERIFY(enter.isAccepted());
        QPointer<QObject> dragOverlay;
        QTRY_VERIFY((dragOverlay = root->findChild<QObject *>("pluginLocalInstallOverlay")));
        QTRY_VERIFY(dragOverlay->property("visible").toBool());
        QTRY_COMPARE(dragOverlay->property("opacity").toReal(), 1.0);
        QTRY_VERIFY(visual(window->contentItem(), "pluginLocalInstallIcon"));
        auto *dragIcon = visual(window->contentItem(), "pluginLocalInstallIcon");
        QTRY_COMPARE(dragIcon->property("status").toInt(), 1);
        if (!output.isEmpty())
            QVERIFY(window->grabWindow().save(output + "/local-install-overlay.png"));
        QDropEvent dropped(QPointF(300, 300), Qt::CopyAction, &draggedPackage, Qt::LeftButton,
                           Qt::NoModifier);
        QCoreApplication::sendEvent(window, &dropped);
        QVERIFY(dropped.isAccepted());
        QTRY_VERIFY(!manager.busy());
        QCOMPARE(host.runtime.state(host.installation()), PluginRuntime::Disabled);
        QTRY_VERIFY(!dragOverlay);
        window->setWidth(650);
        QTest::qWait(100);
        auto *tabs = visual(window->contentItem(), "pluginViewTabs");
        safe = visual(window->contentItem(), "pluginSafeMode");
        QVERIFY(tabs && safe);
        QVERIFY(safe->mapToScene({0, 0}).y() >= tabs->mapToScene({0, 0}).y() + tabs->height());
        QSignalSpy settingNotice(&manager, &PluginManager::notice);
        QVERIFY(!manager.requestSettings(host.installation()));
        QCOMPARE(settingNotice.count(), 1);
        QVERIFY(settingNotice[0][1].toBool());
        if (!output.isEmpty())
            QVERIFY(window->grabWindow().save(output + "/installed-narrow.png"));
        QVERIFY(host.runtime.setSafeMode(false));
        const auto blockedSource = pluginPackage(
            host.temporary.path(), "test.blocked", "throw new Error('must remain blocked');",
            {{"dependencies", QJsonObject{{"test.required-core", ">=1.0.0 <2.0.0"}}}});
        host.runtime.importDirectory(blockedSource);
        QTRY_VERIFY(!host.runtime.loading());
        QCOMPARE(host.runtime.state(host.runtime.installationForId("test.blocked")),
                 PluginRuntime::Blocked);
        window->setWidth(900);
        QTRY_VERIFY(visual(window->contentItem(), "pluginCard-test.blocked"));
        auto *blockedCard = visual(window->contentItem(), "pluginCard-test.blocked");
        QVERIFY(blockedCard->property("warning").toBool());
        QVERIFY(!blockedCard->property("currentFailure").toBool());
        QCOMPARE(visual(blockedCard, "pluginCardStatus")->property("text").toString(),
                 QString("依赖未就绪"));
        if (!output.isEmpty()) {
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                host.theme.setMode(mode);
                QTest::qWait(100);
                QVERIFY(window->grabWindow().save(output + (mode == ThemeStore::Light
                                                                ? "/dependency-light.png"
                                                                : "/dependency-dark.png")));
            }
        }
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        root.reset();
        manager.shutdown();
    }

    void statisticsUseSampledWireAndRejectInvalidNumbers()
    {
        QFile sample(
            QFINDTESTDATA("../../docs/internal/verification/plugin-source-samples/stats.body"));
        QVERIFY(sample.open(QIODevice::ReadOnly));
        const auto bytes = sample.readAll();
        QString error;
        const QString key =
            "github:hoowhoami/echomusicnextplugins:org.echomusicnext.volume-presets";
        auto stats = PluginMarketplaceStats::parse(bytes, {key}, error);
        QVERIFY(error.isEmpty());
        QCOMPARE(stats.size(), 1);
        QCOMPARE(stats.value(key).toMap().value("installCount").toInt(), 0);
        QCOMPARE(PluginMarketplaceStats::parse(bytes, {}, error).size(), 0);
        auto invalid = bytes;
        invalid.replace("\"installCount\":0", "\"installCount\":\"0\"");
        stats = PluginMarketplaceStats::parse(invalid, {key}, error);
        QVERIFY(!error.isEmpty());
        QVERIFY(stats.isEmpty());
    }

    void marketplaceInstallsActualPackageAndRejectsTampering()
    {
        Host host;
        PluginManager manager(host.runtime, host.settings);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QFile manifestFile(QFINDTESTDATA("../fixtures/plugins/marketplace/manifest.json"));
        QVERIFY(manifestFile.open(QIODevice::ReadOnly));
        auto manifest = manifestFile.readAll();
        QFile archiveFile(QFINDTESTDATA("../fixtures/plugins/marketplace/archive.echomod"));
        QVERIFY(archiveFile.open(QIODevice::ReadOnly));
        auto archive = archiveFile.readAll();
        QJsonObject reportedEvent;
        bool holdCatalog = true;
        QList<QPointer<QTcpSocket>> heldRequests;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                const auto request = socket->property("request").toByteArray() + socket->readAll();
                socket->setProperty("request", request);
                if (!request.contains("\r\n\r\n"))
                    return;
                const auto headerEnd = request.indexOf("\r\n\r\n") + 4;
                const auto length = QRegularExpression("(?im)^content-length: *([0-9]+)")
                                        .match(QString::fromLatin1(request.left(headerEnd)))
                                        .captured(1)
                                        .toInt();
                if (request.size() < headerEnd + length)
                    return;
                if (holdCatalog) {
                    if (!heldRequests.contains(socket))
                        heldRequests.append(socket);
                    return;
                }
                const auto requestBody =
                    QJsonDocument::fromJson(request.mid(request.indexOf("\r\n\r\n") + 4)).object();
                if (requestBody.contains("event"))
                    reportedEvent = requestBody;
                const auto body =
                    request.startsWith("GET /echo-plugins.json ")
                        ? QJsonDocument(
                              QJsonObject{
                                  {"schemaVersion", 2},
                                  {"plugins",
                                   QJsonArray{QJsonObject{
                                       {"path", "plugins/fixture"},
                                       {"artifact", "artifacts/fixture.echomod"},
                                       {"sha256",
                                        QString::fromLatin1(QCryptographicHash::hash(
                                                                archive, QCryptographicHash::Sha256)
                                                                .toHex())}}}}})
                              .toJson()
                    : request.startsWith("GET /archive.echomod ") ? archive
                    : request.startsWith("POST ") ? QByteArray(R"({"ok":true,"plugins":[]})")
                                                  : manifest;
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) +
                    "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            });
        });
        manager.m_resolveUrl = [&](const QUrl &url) {
            return QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) +
                        (url.path().endsWith(".echomod")            ? "/archive.echomod"
                         : url.path().endsWith("echo-plugins.json") ? "/echo-plugins.json"
                         : url.path().contains("/v1/")              ? "/stats"
                                                                    : "/manifest.json"));
        };
        host.router.registerPage({"plugin-management",
                                  "/main/settings/plugins",
                                  QUrl("qrc:/qt/qml/EchoMusic/Plugins/PluginManagementPage.qml"),
                                  {"view"},
                                  true,
                                  false,
                                  false,
                                  {}});
        QVERIFY(host.router.navigate("plugin-management", {{"view", "marketplace"}}));
        ShellSettings uiSettings(host.settings);
        QQmlComponent ui(&host.engine);
        ui.setData(R"(
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Theme
import EchoMusic.Plugins
import EchoMusic.Shell
ApplicationWindow {
 id: uiWin
 required property PluginManager manager
 required property RouteController router
 required property ShellSettings settings
 property real uiScale: 1
 width:900; height:650; visible:true; color:Theme.main
 Item {
  width: uiWin.width / uiWin.uiScale; height: uiWin.height / uiWin.uiScale
  scale: uiWin.uiScale; transformOrigin: Item.TopLeft
  PluginManagementPage { anchors.fill:parent; manager:uiWin.manager; router:uiWin.router; context:uiWin.router.current; settings:uiWin.settings }
 }
})",
                   QUrl("qrc:/marketplace-ui-test.qml"));
        std::unique_ptr<QObject> uiRoot(
            ui.createWithInitialProperties({{"manager", QVariant::fromValue(&manager)},
                                            {"router", QVariant::fromValue(&host.router)},
                                            {"settings", QVariant::fromValue(&uiSettings)}}));
        QVERIFY2(uiRoot, qPrintable(ui.errorString()));
        auto *uiWindow = qobject_cast<QQuickWindow *>(uiRoot.get());
        QVERIFY(uiWindow && QTest::qWaitForWindowExposed(uiWindow));
        const auto uiOutput = qEnvironmentVariable("ECHO_PLUGIN_UI_OUTPUT");
        QStringList uiWarnings;
        connect(&host.engine, &QQmlEngine::warnings, uiRoot.get(),
                [&](const QList<QQmlError> &errors) {
                    for (const auto &error : errors)
                        uiWarnings.append(error.toString());
                });
        QTRY_VERIFY(visual(uiWindow->contentItem(), "pluginMarketplaceSkeleton"));

        if (!uiOutput.isEmpty()) {
            QDir().mkpath(uiOutput);
            QVERIFY(uiWindow->grabWindow().save(uiOutput + "/marketplace-loading.png"));
        }
        holdCatalog = false;
        for (auto &socket : heldRequests)
            if (socket)
                QVERIFY(QMetaObject::invokeMethod(socket, "readyRead", Qt::QueuedConnection));
        manager.refreshCatalog();
        QTRY_VERIFY(!manager.catalogLoading());
        QCOMPARE(manager.catalogCount(), 1);
        QTRY_VERIFY(visual(uiWindow->contentItem(), "pluginStat-heartbeat"));
        if (!uiOutput.isEmpty()) {
            QDir().mkpath(uiOutput);
            uiWindow->resize(1000, 800);
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                host.theme.setMode(mode);
                for (const auto scale : {1.0, 1.25}) {
                    uiWindow->setProperty("uiScale", scale);
                    QTest::mouseMove(uiWindow, {uiWindow->width() - 4, uiWindow->height() - 4});
                    QTest::qWait(100);
                    const auto name = QString("/marketplace-%1-%2").arg(int(mode)).arg(scale);
                    QVERIFY(uiWindow->grabWindow().save(uiOutput + name + ".png"));
                    auto *surface = visual(uiWindow->contentItem(), "pluginCard-org.test.catalog");
                    QVERIFY(surface);
                    QTest::mouseMove(uiWindow,
                                     surface->mapToScene({surface->width() - 8, 8}).toPoint());
                    QTest::qWait(220);
                    QVERIFY(uiWindow->grabWindow().save(uiOutput + name + "-hover.png"));
                }
            }
            uiWindow->setProperty("uiScale", 1.0);
            host.theme.setMode(ThemeStore::Light);
            host.theme.selectSolid(QColor("#fff0ed"));
            QTest::mouseMove(uiWindow, {uiWindow->width() - 4, uiWindow->height() - 4});
            QTest::qWait(100);
            QVERIFY(uiWindow->grabWindow().save(uiOutput + "/marketplace-tinted.png"));
            host.theme.selectEcho();
            uiWindow->resize(900, 650);
            QTest::mouseMove(uiWindow, {uiWindow->width() - 4, uiWindow->height() - 4});
        }
        auto *sourceAction = visual(uiWindow->contentItem(), "pluginSourceManage");
        QVERIFY(sourceAction);
        QVERIFY(QMetaObject::invokeMethod(sourceAction, "clicked"));
        auto *sourceDialog = uiRoot->findChild<QObject *>("pluginSourceDialog");
        QVERIFY(sourceDialog);
        QTRY_VERIFY(sourceDialog->property("visible").toBool());
        QTRY_COMPARE(sourceDialog->property("opacity").toReal(), 1.0);
        if (!uiOutput.isEmpty()) {
            QTest::qWait(100);
            QVERIFY(uiWindow->grabWindow().save(uiOutput + "/source-dialog.png"));
        }
        QVERIFY2(uiWarnings.isEmpty(), qPrintable(uiWarnings.join('\n')));
        uiRoot.reset();
        const auto key = manager.catalog()
                             ->data(manager.catalog()->index(0), Qt::UserRole + 1)
                             .toMap()
                             .value("key")
                             .toString();
        QSignalSpy notice(&manager, &PluginManager::notice);
        manager.share(key);
        const QUrl shareUrl(QGuiApplication::clipboard()->text());
        QVERIFY(shareUrl.scheme() == "echomusicnext");
        SharingCommands publisher(host.account);
        manager.attachSharing(publisher);
        manager.share(key);
        const auto sharedTarget = parseShareTarget(QGuiApplication::clipboard()->text());
        QVERIFY(sharedTarget && sharedTarget->type == ShareResource::Plugin);
        QCOMPARE(sharedTarget->id, QString("org.test.catalog"));
        QVERIFY(QGuiApplication::clipboard()->text().contains(
            "https://hoowhoami.github.io/EchoMusicNext/share/"));
        QVERIFY(manager.patchSource("github:hoowhoami/echomusicnextplugins", "Private local label",
                                    false));
        auto versionDriftShare = *sharedTarget;
        versionDriftShare.query["version"] = "2.0.0";
        QVERIFY(manager.openShare(versionDriftShare));
        QCOMPARE(manager.shareState(), "need-source");
        QVERIFY(manager.acceptShareSource());
        QTRY_COMPARE(manager.shareState(), "ready");
        QCOMPARE(manager.shareRecord().value("shareWarning").toString(),
                 "分享时版本为 v2.0.0，当前插件源提供 v1.0.0。");
        ShellSettings shellSettings(host.settings);
        QQmlComponent sharePage(&host.engine,
                                QUrl("qrc:/qt/qml/EchoMusic/Plugins/PluginShareResolvePage.qml"));
        std::unique_ptr<QObject> shareRoot(sharePage.createWithInitialProperties(
            {{"manager", QVariant::fromValue(&manager)},
             {"router", QVariant::fromValue(&host.router)},
             {"context", QVariant::fromValue(host.router.current())},
             {"settings", QVariant::fromValue(&shellSettings)}}));
        QVERIFY2(shareRoot, qPrintable(sharePage.errorString()));
        QVERIFY(manager.installShared());
        QTRY_VERIFY(!manager.busy());
        QTRY_VERIFY(!reportedEvent.isEmpty());
        QVERIFY(!QJsonDocument(reportedEvent).toJson().contains("Private local label"));
        QCOMPARE(reportedEvent.value("plugin").toObject().value("sourceName").toString(),
                 QString("hoowhoami/EchoMusicNextPlugins"));
        QTRY_COMPARE(manager.shareState(), "installed");
        QVERIFY(!manager.openShare(
            QUrl("echomusicnext://plugin?source=file:///tmp&id=x&version=1.0.0&path=x")));
        const auto installation = host.runtime.installationForId("org.test.catalog");
        QVERIFY(!installation.isEmpty());
        QCOMPARE(host.runtime.state(installation), PluginRuntime::Active);
        auto *command = host.runtime.executeCommand(installation, "value");
        QTRY_COMPARE(command->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(command->result().toInt(), 7);
        QVERIFY(QFileInfo(host.runtime.directory() + "/data/" + installation + "/values.sqlite")
                    .isFile());
        const auto originalArchive = archive;
        const auto generation = host.runtime.context(installation)->activationGeneration();
        // The index has already bound the original bytes. Even an otherwise
        // extractable container must fail before replacing any active code.
        archive.append("tampered");
        QVERIFY(manager.install(key));
        QTRY_VERIFY(!manager.busy());
        QVERIFY(notice.last()[1].toBool());
        QVERIFY(notice.last()[0].toString().contains("SHA-256"));
        QCOMPARE(host.runtime.context(installation)->activationGeneration(), generation);
        archive = originalArchive;
        manifest.replace("1.0.0\"", "1.1.0\"");
        manager.refreshCatalog(true);
        QTRY_VERIFY(!manager.catalogLoading());
        QCOMPARE(manager.updateCount(), 1);
        QVERIFY(manager.install(key));
        QTRY_VERIFY(!manager.busy());
        QVERIFY(notice.last()[1].toBool());
        QCOMPARE(host.runtime.record(installation).value("version").toString(), "1.0.0");
        QCOMPARE(host.runtime.state(installation), PluginRuntime::Active);
        manager.shutdown();
    }
    void spectrumCapabilityAndOwnerLifetime()
    {
        Host host;
        const auto source = package(host.temporary.path(), "export function activate(ctx) {}",
                                    {"lifecycle", "audio.spectrum"});
        host.runtime.importDirectory(source);
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        QVERIFY(!host.runtime.context(host.installation())->audio()->spectrum()->subscribe());
        QCOMPARE(host.playback.spectrum()->subscriberCount(), 0);
        auto owner = std::make_unique<QObject>();
        const auto id =
            host.playback.spectrum()->subscribe(owner.get(), {{"fps", 999}, {"fftSize", 300}});
        QVERIFY(id);
        QCOMPARE(PlayerSpectrum::normalize({{"fps", 999}, {"fftSize", 300}}).value("fps").toInt(),
                 60);
        QCOMPARE(
            PlayerSpectrum::normalize({{"fps", 999}, {"fftSize", 300}}).value("fftSize").toInt(),
            2048);
        QCOMPARE(host.playback.spectrum()->subscriberCount(), 1);
        owner.reset();
        QCOMPARE(host.playback.spectrum()->subscriberCount(), 0);
    }
    void builtAuthorPackageCommands()
    {
        const auto source = qEnvironmentVariable("ECHO_AUTHOR_PLUGIN_PACKAGE");
        if (source.isEmpty())
            QSKIP("Set ECHO_AUTHOR_PLUGIN_PACKAGE to the built spectrum-visualizer .echomod");
        Host host;
        host.shell.attachPlayback(host.playback);
        host.shell.attachTheme(host.theme);
        host.theme.setMotionEnabled(false);
        const auto originalVolume = host.playback.volume();
        host.runtime.importDirectory(source);
        QTRY_VERIFY(!host.runtime.loading());
        const auto installation = host.runtime.installationForId("spectrum-visualizer");
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Active);
        auto *scope = host.runtime.context(installation);
        QList<QPointer<EchoPluginOperation>> operations;
        QList<QJSValue> operationRoots;
        auto cleanupOperations = qScopeGuard([&] {
            for (const auto &op : operations)
                if (op)
                    delete op.data();
        });
        // A native QPointer does not root a JavaScript-owned completed result.
        // Screenshot/render cycles can collect it between QTRY evaluations.
        const auto retain = [&](EchoPluginOperation *op) {
            // Completion deliberately switches ownership on the next tick.
            // A persistent JS wrapper roots the result through that transition.
            operationRoots.append(host.engine.newQObject(op));
            operations.append(op);
            return QPointer<EchoPluginOperation>(op);
        };
        QCOMPARE(host.playback.volume(), originalVolume);
        QTRY_COMPARE(host.shell.pluginSurfaces()->entries().size(), 3);
        QCOMPARE(scope->ui()->state().value("showLyricControls").toBool(), true);
        QCOMPARE(scope->ui()->state().value("showPlayerBar").toBool(), false);
        QPointer<EchoPluginOperation> patch = retain(scope->executeCommand(
            "settings.patch",
            {{"showPlayerBar", true}, {"showMiniPlayer", true}, {"mode", "hybrid"}, {"fps", 30}}));
        QTRY_VERIFY(patch && patch->finished());
        QCOMPARE(patch->state(), EchoPluginOperation::Succeeded);
        QPointer<EchoPluginOperation> stored = retain(scope->io()->readValue("settings"));
        QTRY_COMPARE(stored->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(stored->result().toMap().value("mode").toString(), "hybrid");
        QVERIFY(!scope->ui()->settingsComponent().isEmpty());
        QStringList warnings;
        connect(&host.engine, &QQmlEngine::warnings, &host.engine,
                [&](const QList<QQmlError> &errors) {
                    for (const auto &error : errors)
                        warnings.append(error.toString());
                });
        FloatingPlayerController floating(host.settings, false);
        floating.toggleMini();
        QQmlComponent component(&host.engine);
        component.setData(R"(
import QtQuick
import QtQuick.Controls.Basic
import EchoMusic.Shell
import EchoMusic.Playback
import EchoMusic.Theme
import EchoMusic.Controls
import EchoMusic.Plugins
import EchoMusic.PluginSdk
ApplicationWindow {
    id: win
    required property ShellController shell
    required property RouteController router
    required property PlaybackService playback
    required property FloatingPlayerController floating
    required property PluginRuntime runtime
    required property EchoPluginContext scope
    required property string installation
    function openSettings() { settingsOwner.open(); }
    function closeSettings() { settingsOwner.close(); }
    EchoPopupLoader {
        id: settingsOwner
        hostItem: win.contentItem
        sourceComponent: Component {
            PluginSettingsDialog { runtime: win.runtime; scope: win.scope; installationId: win.installation; uiScale: win.uiScale }
        }
    }
    property real uiScale: 1
    width: 1100; height: 340; visible: true; color: Theme.main
    Item {
        width: win.width / win.uiScale; height: win.height / win.uiScale
        scale: win.uiScale; transformOrigin: Item.TopLeft
        PlayerBar { width: parent.width; y: 20; controller: win.shell; router: win.router; uiScale: win.uiScale; manageCapacity: false }
        PlayerBar { width: parent.width; y: 230; lyricStyle: true; controller: win.shell; router: win.router; uiScale: win.uiScale; manageCapacity: false }
    }
    MiniPlayerWindow { objectName: "spectrumMini"; service: win.floating; playback: win.playback; pluginSurfaces: win.shell.pluginSurfaces }
})",
                          QUrl("qrc:/spectrum-production-ui.qml"));
        std::unique_ptr<QObject> root(component.createWithInitialProperties(
            {{"shell", QVariant::fromValue(&host.shell)},
             {"router", QVariant::fromValue(&host.router)},
             {"playback", QVariant::fromValue(&host.playback)},
             {"floating", QVariant::fromValue(&floating)},
             {"runtime", QVariant::fromValue(&host.runtime)},
             {"scope", QVariant::fromValue(scope)},
             {"installation", installation}}));
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.get());
        auto *mini = root->findChild<QQuickWindow *>("spectrumMini");
        QVERIFY(window && mini);
        QTest::qWait(200);
        std::function<void(QQuickItem *)> describe = [&](QQuickItem *item) {
            if (item->objectName() == "spectrumVisualizerLayer" ||
                QString(item->metaObject()->className()).contains("PluginSurfaceHost"))
                qInfo() << item->metaObject()->className() << item->property("target")
                        << item->property("placement") << item->size() << item->implicitHeight()
                        << item->isVisible() << item->property("live") << item->property("allowed");
            for (auto *child : item->childItems())
                describe(child);
        };
        describe(window->contentItem());
        describe(mini->contentItem());
        QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 3);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QTRY_VERIFY_WITH_TIMEOUT(host.playback.engineReady(), 15000);
        // Quiet, real local PCM passes through the production decoder, mixer,
        // output callback, FFT, FFI and typed subscriber; no injected frames.
        QByteArray pcm;
        QDataStream data(&pcm, QIODevice::WriteOnly);
        data.setByteOrder(QDataStream::LittleEndian);
        constexpr int rate = 48000, seconds = 60;
        for (int i = 0; i < rate * seconds; ++i)
            data << qint16(std::sin(i * 2.0 * 3.141592653589793 * 1000 / rate) * 12000);
        QByteArray wave;
        QDataStream header(&wave, QIODevice::WriteOnly);
        header.setByteOrder(QDataStream::LittleEndian);
        header.writeRawData("RIFF", 4);
        header << quint32(36 + pcm.size());
        header.writeRawData("WAVEfmt ", 8);
        header << quint32(16) << quint16(1) << quint16(1) << quint32(rate) << quint32(rate * 2)
               << quint16(2) << quint16(16);
        header.writeRawData("data", 4);
        header << quint32(pcm.size());
        wave.append(pcm);
        const auto path = host.temporary.path() + "/1000hz.wav";
        write(path, wave);
        PlaybackTrack track;
        track.id = "spectrum-tone";
        track.title = "1000 Hz";
        track.localPath = path;
        track.durationMs = seconds * 1000;
        host.playback.setVolume(8);
        auto restore = qScopeGuard([&] { host.playback.setVolume(originalVolume); });
        QPointer<EchoPluginSpectrumSubscription> sub =
            scope->audio()->spectrum()->subscribe({{"fps", 30},
                                                   {"binCount", 512},
                                                   {"fftSize", 8192},
                                                   {"scale", "linear"},
                                                   {"smoothing", 0.0},
                                                   {"includeWaveform", true}});
        QVERIFY(sub && sub->active());
        QSignalSpy frames(sub, &EchoPluginSpectrumSubscription::frameChanged);
        host.playback.playTracks({track}, 0, "Spectrum test", "spectrum-test");
        QTRY_VERIFY2_WITH_TIMEOUT(host.playback.playing() && !host.playback.loading(),
                                  qPrintable(host.playback.errorMessage()), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(sub->frame().toMap().value("source") == "player" &&
                                     sub->frame().toMap().value("rms").toDouble() > 0.005 &&
                                     sub->frame().toMap().value("fftSize").toInt() == 8192,
                                 10000);
        auto frame = sub->frame().toMap();
        const auto bins = frame.value("bins").toList();
        QCOMPARE(bins.size(), 512);
        QCOMPARE(frame.value("fftSize").toInt(), 8192);
        QVERIFY(frame.value("sampleRate").toInt() >= 16000);
        QVERIFY(frame.value("rms").toDouble() > 0.005);
        QVERIFY(frame.value("peak").toDouble() > frame.value("rms").toDouble());
        QCOMPARE(frame.value("waveform").toList().size(), 8192);
        auto maximum = std::max_element(bins.begin(), bins.end(),
                                        [](auto a, auto b) { return a.toDouble() < b.toDouble(); });
        const auto peakHz = 20 + (std::distance(bins.begin(), maximum) + .5) * (20000 - 20) / 512;
        QVERIFY2(std::abs(peakHz - 1000) < 150, qPrintable(QString::number(peakHz)));
        qInfo() << "Actual output PCM" << frame.value("sampleRate") << "FFT"
                << frame.value("fftSize") << "peak Hz" << peakHz << "RMS" << frame.value("rms");
        host.playback.pause();
        QTRY_VERIFY(!host.playback.spectrum()->status().value("running").toBool());
        QTRY_COMPARE(sub->frame().toMap().value("source").toString(), "silence");
        frames.clear();
        QTest::qWait(120);
        QCOMPARE(frames.count(), 0);
        host.playback.play();
        QTRY_VERIFY(host.playback.spectrum()->status().value("running").toBool());
        QTRY_COMPARE(sub->frame().toMap().value("source").toString(), "player");
        sub->setPaused(true);
        frames.clear();
        QTest::qWait(120);
        QCOMPARE(frames.count(), 0);
        sub->setPaused(false);
        QTRY_VERIFY(frames.count() > 0);
        sub->dispose();
        QTRY_VERIFY(sub.isNull());
        // Opt-in Release measurement of the real output/SDK/production surfaces.
        // Process CPU includes every thread; setup, screenshots and SQL writes
        // are excluded from each steady-state interval.
        const int benchmarkMs = qEnvironmentVariableIntValue("ECHO_SPECTRUM_BENCHMARK_MS");
        if (benchmarkMs > 0) {
            host.theme.setMode(ThemeStore::Dark);
            QSignalSpy delivered(host.playback.spectrum(), &PlayerSpectrum::frameReady);
            QSignalSpy rendered(window, &QQuickWindow::frameSwapped);
            QSignalSpy miniRendered(mini, &QQuickWindow::frameSwapped);
            const auto measure = [&](const QString &label) {
                if (window->isVisible())
                    QVERIFY(Test::prepareNativeInput(window));
                QTest::qWait(500);
                delivered.clear();
                rendered.clear();
                miniRendered.clear();
                QElapsedTimer elapsed;
                elapsed.start();
                const auto cpu = std::clock();
                QTest::qWait(qBound(1000, benchmarkMs, 20000));
                const auto wall = elapsed.elapsed();
                const auto cpuMs = 1000. * (std::clock() - cpu) / CLOCKS_PER_SEC;
                qInfo().noquote() << QString("SPECTRUM_CPU %1 wall_ms=%2 cpu_ms=%3 percent=%4 "
                                             "frames=%5 rendered=%6 mini_rendered=%7")
                                         .arg(label)
                                         .arg(wall)
                                         .arg(cpuMs, 0, 'f', 1)
                                         .arg(cpuMs * 100 / wall, 0, 'f', 2)
                                         .arg(delivered.count())
                                         .arg(rendered.count())
                                         .arg(miniRendered.count());
                if (label.contains("fps") && !label.startsWith("disabled")) {
                    QVERIFY(host.playback.playing() && !host.playback.loading());
                    QCOMPARE(host.playback.spectrum()->subscriberCount(), 3);
                    const auto minimum =
                        host.runtime.context(installation)->ui()->state().value("fps").toInt() *
                        wall / 1000 / 3;
                    // Baseline saturation is itself a measured regression; keep
                    // recording it, but never silently count a stalled interval
                    // as a successful CPU reduction.
                    if (delivered.count() < minimum * 3 || rendered.count() < minimum ||
                        miniRendered.count() < minimum)
                        qInfo().noquote() << "SPECTRUM_STALLED" << label;
                }
            };
            for (const auto fps : {15, 30}) {
                for (const auto &mode :
                     {"disabled", "bars", "wave", "hybrid", "mist", "centered"}) {
                    patch = retain(scope->executeCommand("settings.patch",
                                                         {{"enabled", QString(mode) != "disabled"},
                                                          {"mode", mode},
                                                          {"fps", fps},
                                                          {"showBackdrop", true}}));
                    QTRY_VERIFY(patch && patch->finished());
                    QCOMPARE(patch->state(), EchoPluginOperation::Succeeded);
                    measure(QString("%1-%2fps").arg(mode).arg(fps));
                }
            }
            host.playback.pause();
            QTRY_VERIFY(!host.playback.spectrum()->status().value("running").toBool());
            measure("paused");
            window->hide();
            mini->hide();
            QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 0);
            measure("hidden");
            return;
        }
        const auto artifacts = qEnvironmentVariable("ECHO_SPECTRUM_ARTIFACTS");
        if (!artifacts.isEmpty())
            QDir().mkpath(artifacts);
        for (const auto &mode : {"bars", "wave", "hybrid", "mist", "centered"}) {
            patch = retain(
                scope->executeCommand("settings.patch", {{"mode", mode}, {"showBackdrop", true}}));
            QTRY_VERIFY(patch && patch->finished());
            QCOMPARE(patch->state(), EchoPluginOperation::Succeeded);
            QTest::qWait(1);
            host.engine.collectGarbage();
            QVERIFY(patch);
            QCOMPARE(patch->state(), EchoPluginOperation::Succeeded);
            for (const auto &theme : {"dark", "light"}) {
                host.theme.setMode(QString(theme) == "dark" ? ThemeStore::Dark : ThemeStore::Light);
                for (double scale : {1.0, 1.25}) {
                    window->setProperty("uiScale", scale);
                    QTest::qWait(180);
                    auto *layer = visual(window->contentItem(), "spectrumVisualizerLayer");
                    auto *miniLayer = visual(mini->contentItem(), "spectrumVisualizerLayer");
                    QVERIFY(layer && miniLayer);
                    QVERIFY(layer->property("frame").toMap().value("source") == "player");
                    QVERIFY(!window->grabWindow().isNull());
                    int curveCount = 0;
                    std::function<void(QQuickItem *)> verifyVectors = [&](QQuickItem *item) {
                        QVERIFY(!QString(item->metaObject()->className()).contains("Canvas"));
                        if (item->property("preferredRendererType").isValid()) {
                            QCOMPARE(item->property("preferredRendererType").toInt(), 4);
                            if (window->rendererInterface()->graphicsApi() ==
                                QSGRendererInterface::Metal)
                                QCOMPARE(item->property("rendererType").toInt(), 4);
                            ++curveCount;
                        }
                        for (auto *child : item->childItems())
                            verifyVectors(child);
                    };
                    verifyVectors(layer);
                    verifyVectors(miniLayer);
                    QVERIFY(curveCount >= 2);
                    if (!artifacts.isEmpty()) {
                        const auto name = QString("/%1-%2-%3.png").arg(mode, theme).arg(scale);
                        QVERIFY(window->grabWindow().save(artifacts + name));
                        QVERIFY(mini->grabWindow().save(artifacts + "/mini" + name.mid(1)));
                    }
                }
            }
        }
        host.playback.pause();
        QTRY_VERIFY(!host.playback.spectrum()->status().value("running").toBool());
        auto *centerLayer = visual(window->contentItem(), "spectrumVisualizerLayer");
        QVERIFY(centerLayer);
        auto *centerClock = centerLayer->findChild<QObject *>("spectrumRenderClock");
        QVERIFY(centerClock);
        QTRY_VERIFY_WITH_TIMEOUT(!centerClock->property("running").toBool(), 3000);
        host.playback.play();
        QTRY_VERIFY(centerClock->property("running").toBool());
        // Exercise the same demand-created settings dialog as the management page.
        window->resize(1100, 900);
        for (const auto mode : {ThemeStore::Dark, ThemeStore::Light}) {
            host.theme.setMode(mode);
            for (const auto scale : {1.0, 1.25}) {
                window->setProperty("uiScale", scale);
                QVERIFY(QMetaObject::invokeMethod(root.get(), "openSettings"));
                QTRY_VERIFY(visual(window->contentItem(), "spectrumVisualizerSettings"));
                QPointer<QQuickItem> body =
                    visual(window->contentItem(), "spectrumVisualizerSettings");
                QTest::qWait(120);
                QVERIFY(body->width() > 400);
                if (mode == ThemeStore::Dark && scale == 1.0) {
                    QVERIFY(Test::prepareNativeInput(window));
                    auto *toggle = visual(body, "spectrumEnabledSwitch");
                    QVERIFY(toggle);
                    auto click = [&] {
                        const auto point =
                            toggle->mapToScene({toggle->width() / 2, toggle->height() / 2})
                                .toPoint();
                        Test::moveNativePointer(window, point);
                        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
                    };
                    click();
                    QTRY_VERIFY(!scope->ui()->state().value("enabled").toBool());
                    QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 0);
                    click();
                    QTRY_VERIFY(scope->ui()->state().value("enabled").toBool());
                    QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 3);
                }

                QPointer<QQuickItem> viewport =
                    visual(window->contentItem(), "pluginSettingsViewport");
                QVERIFY(viewport);
                QVERIFY(viewport->property("contentHeight").toReal() > viewport->height());
                QVERIFY(qAbs(viewport->width() - body->width() - 24) < .01);
                QList<QQuickItem *> rows;
                std::function<void(QQuickItem *)> collectRows = [&](QQuickItem *item) {
                    if (item->objectName() == "spectrumSettingsRow")
                        rows.append(item);
                    for (auto *child : item->childItems())
                        collectRows(child);
                };
                collectRows(body);
                QVERIFY(rows.size() >= 5);
                QVERIFY(!visual(rows[0], "spectrumSettingsDivider")->isVisible());
                for (int i = 1; i < 5; ++i) {
                    auto *divider = visual(rows[i], "spectrumSettingsDivider");
                    QVERIFY(divider && divider->isVisible());
                    QCOMPARE(divider->y(),
                             0.0); // Source separators are between rows, never below the group.
                }
                if (!artifacts.isEmpty())
                    QVERIFY(window->grabWindow().save(
                        artifacts + QString("/settings-%1-%2.png").arg(int(mode)).arg(scale)));
                QVERIFY(Test::prepareNativeInput(window));
                ulong wheelTimestamp = 10000;
                const auto wheel = [&](QPoint delta, Qt::ScrollPhase phase) {
                    const auto point =
                        viewport->mapToScene({viewport->width() * .2, viewport->height() * .5});
                    QWheelEvent event(point, window->mapToGlobal(point.toPoint()), delta,
                                      delta.isNull() ? QPoint{} : delta * 2, Qt::NoButton,
                                      Qt::NoModifier, phase, false,
                                      Qt::MouseEventSynthesizedBySystem);
                    event.setTimestamp(wheelTimestamp += 16);
                    QCoreApplication::sendEvent(window, &event);
                };
                wheel({}, Qt::ScrollBegin);
                for (int i = 0; i < 80 && !viewport->property("atYEnd").toBool(); ++i) {
                    wheel({0, -100}, Qt::ScrollUpdate);
                    QTest::qWait(16);
                }
                wheel({}, Qt::ScrollEnd);
                QTRY_VERIFY(viewport->property("contentY").toReal() > 0);
                QTRY_VERIFY(viewport->property("atYEnd").toBool());
                auto *reset = visual(body, "spectrumResetSettings");
                QVERIFY(reset);
                const auto resetTop = reset->mapToItem(viewport, {0, 0}).y();
                QVERIFY(resetTop >= 0 && resetTop + reset->height() <= viewport->height() + 1);
                auto *scrollbar = visual(window->contentItem(), "popupScrollBar");
                QVERIFY(scrollbar && scrollbar->isVisible());
                qInfo() << "Settings wheel reached bottom at scale" << scale << "contentY"
                        << viewport->property("contentY");
                if (!artifacts.isEmpty())
                    QVERIFY(window->grabWindow().save(
                        artifacts +
                        QString("/settings-bottom-%1-%2.png").arg(int(mode)).arg(scale)));
                QVERIFY(QMetaObject::invokeMethod(root.get(), "closeSettings"));
                QTRY_VERIFY(body.isNull());
                QTRY_VERIFY(viewport.isNull());
            }
        }
        if (QGuiApplication::platformName() == "cocoa") {
            window->showMinimized();
            QTRY_COMPARE(window->visibility(), QWindow::Minimized);
            QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 1);
            window->showNormal();
            QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 3);
        }
        window->hide();
        mini->hide();
        QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 0);
        QTRY_VERIFY(!host.playback.spectrum()->status().value("running").toBool());
        QVERIFY(!visual(window->contentItem(), "spectrumVisualizerLayer"));
        for (int i = 0; i < 3; ++i) {
            window->show();
            mini->show();
            QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 3);
            window->hide();
            mini->hide();
            QTRY_COMPARE(host.playback.spectrum()->subscriberCount(), 0);
        }
        patch = retain(scope->executeCommand("settings.reset"));
        QTRY_VERIFY(patch && patch->finished());
        QCOMPARE(patch->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(scope->ui()->state().value("mode").toString(), "bars");
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        QPointer<EchoPluginContext> retired = scope;
        QVERIFY(host.runtime.setEnabled(installation, false));
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Disabled);
        QTRY_VERIFY(retired.isNull());
        QCOMPARE(host.shell.pluginSurfaces()->entries().size(), 0);
        QCOMPARE(host.playback.spectrum()->subscriberCount(), 0);
        QVERIFY(host.runtime.setEnabled(installation, true));
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Active);
        QCOMPARE(host.runtime.context(installation)->ui()->state().value("mode").toString(),
                 "bars");
        QCOMPARE(host.runtime.context(installation)->ui()->state().value("showPlayerBar").toBool(),
                 false);
        QVERIFY(host.runtime.setEnabled(installation, false));
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Disabled);
        QCOMPARE(host.playback.spectrum()->subscriberCount(), 0);
    }
    void packageIntegrityAndCompatibility()
    {
        QTemporaryDir directory;
        const auto source = package(directory.path(), "export function activate(ctx) {}\n");
        const auto first =
            PluginPackage::importDirectory(source, directory.path() + "/packages", "identity");
        QVERIFY2(first.error.isEmpty(), qPrintable(first.error));
        QVERIFY(first.root != source);
        QCOMPARE(PluginPackage::importDirectory(source, directory.path() + "/packages", "identity")
                     .contentHash,
                 first.contentHash);
        write(source + "/entry.mjs", "export function activate(ctx) { return 1; }\n");
        const auto second =
            PluginPackage::importDirectory(source, directory.path() + "/packages", "identity");
        QVERIFY(second.root != first.root);
        QVERIFY(first.resolve("../entry.mjs").isEmpty());
        write(source + "/qmldir", "module Unsafe\nplugin malicious\n");
        QVERIFY(!PluginPackage::importDirectory(source, directory.path() + "/packages", "identity")
                     .error.isEmpty());
        QFile::remove(source + "/qmldir");
        package(directory.path(), "throw new Error('must not execute');", {"playback.audioSource"});
        QVERIFY(PluginPackage::importDirectory(source, directory.path() + "/packages", "identity")
                    .incompatible);
    }
    void backupBarrierDrainsScopesWithoutChangingEnabledPreference()
    {
        Host host;
        const auto source = package(host.temporary.path(), R"JS(
import { awaitOperation } from 'echo:plugin-sdk';
export function activate(ctx) {
  return awaitOperation(ctx.io.writeValue('backup-value',{value:42}));
}
)JS",
                                    {"storage"});
        host.runtime.importDirectory(source);
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        const auto installation = host.installation();
        bool ready = false;
        QVERIFY(host.runtime.beginMaintenance([&](bool result) { ready = result; }));
        QTRY_VERIFY(ready);
        QVERIFY(!host.runtime.context(installation));
        QCOMPARE(host.runtime.enabledCount(), 1);
        QVERIFY(!host.runtime.setEnabled(installation, false));
        QVERIFY(!host.runtime.reload(installation));
        const auto entries = host.runtime.backupEntries();
        QCOMPARE(entries.size(), 1);
        QVERIFY(!entries.first().toMap().value("origin").toMap().contains("directory"));
        const auto captured = host.temporary.path() + "/data-snapshot";
        const auto result = PluginDataTransaction::capture(
            host.runtime.directory() + "/data/" + installation, captured);
        QVERIFY2(result.isEmpty(), qPrintable(result));
        QVERIFY(QFile::exists(captured + "/values.sqlite"));
        const auto connection = QStringLiteral("backup-barrier-snapshot");
        auto cleanup = qScopeGuard([&] { QSqlDatabase::removeDatabase(connection); });
        {
            auto database = QSqlDatabase::addDatabase("QSQLITE", connection);
            database.setDatabaseName(captured + "/values.sqlite");
            database.setConnectOptions("QSQLITE_OPEN_READONLY");
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec("SELECT json FROM plugin_values WHERE key='backup-value'"));
            QVERIFY(query.next());
            QCOMPARE(QJsonDocument::fromJson(query.value(0).toByteArray())
                         .object()
                         .value("value")
                         .toObject()
                         .value("value")
                         .toInt(),
                     42);
        }
        host.runtime.endMaintenance();
        QTRY_COMPARE(host.runtime.state(installation), PluginRuntime::Active);
        QCOMPARE(host.runtime.enabledCount(), 1);
    }
    void realActivationSdkAndRetirement()
    {
        Host host;
        QAbstractItemModelTester manager(&host.runtime,
                                         QAbstractItemModelTester::FailureReportingMode::QtTest);
        const auto source = package(
            host.temporary.path(), R"JS(
import { awaitOperation } from 'echo:plugin-sdk';
export function activate(ctx) {
  const oldPage = ctx.ui.registerPage('tools','Tools.qml');
  ctx.ui.registerPage('tools','Tools.qml');
  oldPage.dispose();
  const old = ctx.ui.registerTitlebarAction('action','Old','settings',()=>ctx.player.setVolume(90));
  ctx.ui.registerTitlebarAction('action','Plugin volume','settings',()=>ctx.player.setVolume(30));
  old.dispose();
  ctx.registerCommand('read',()=>awaitOperation(ctx.io.readValue('answer')));
  ctx.registerCommand('slow',()=>awaitOperation(ctx.io.writeValue('slow',123)));
  ctx.setTimer('volume',60000,true,()=>ctx.player.setVolume(80));
  ctx.onPlayerChanged('observe',()=>{});
  ctx.ui.createTask('task','Plugin task').setProgress(25,'Loading');
  ctx.onDispose(()=>{ throw new Error('cleanup failure must not skip others'); });
  return awaitOperation(ctx.io.writeValue('answer',{value:42}))
    .then(() => awaitOperation(ctx.player.setVolume(60)));
}
export function deactivate(ctx) {
  if (ctx.active) throw new Error('scope still active during deactivate');
}
)JS",
            {"storage", "ui.pages", "ui.titlebar", "tasks", "playback.controls", "commands"});
        write(source + "/Tools.qml", R"QML(import QtQuick
import EchoMusic.PluginSdk 1.0
EchoPluginPage { property string caption: plugin.player.title }
)QML");
        host.runtime.importDirectory(source);
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        auto *context = host.runtime.context(host.installation());
        QVERIFY(context);
        QCOMPARE(host.playback.volume(), 60);
        QVERIFY(host.router.available("test.runtime:page:tools"));
        QCOMPARE(host.shell.tasks()->count(), 1);
        auto *read = host.runtime.executeCommand(host.installation(), "read");
        QVERIFY(read);
        QTRY_COMPARE(read->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(read->result().toMap().value("value").toInt(), 42);
        QVERIFY(host.shell.titlebarActions()->activate("test.runtime:titlebar:action"));
        QTRY_COMPARE(host.playback.volume(), 30);
        QVERIFY(context->ui()->navigate("tools")->state() == EchoPluginOperation::Succeeded);
        QCOMPARE(host.router.current()->name(), "test.runtime:page:tools");
        QQmlComponent component(&host.engine, host.router.current()->component());
        QTRY_VERIFY(component.status() != QQmlComponent::Loading);
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));
        const auto props = host.router.current()->initialProperties();
        auto initial = props;
        initial.insert("context", QVariant::fromValue(host.router.current()));
        QScopedPointer<QObject> page(component.createWithInitialProperties(initial));
        QVERIFY2(page, qPrintable(component.errorString()));
        QVERIFY(!props.contains("router") && !props.contains("settings"));
        auto *oldSession = context->player()->setVolume(99);
        host.api.advanceSession();
        QCOMPARE(oldSession->state(), EchoPluginOperation::Canceled);
        auto *pending = context->player()->setVolume(95);
        QVERIFY(host.runtime.setEnabled(host.installation(), false));
        QCOMPARE(pending->state(), EchoPluginOperation::Canceled);
        QVERIFY(!host.shell.titlebarActions()->activate("test.runtime:titlebar:action"));
        QVERIFY(!host.router.available("test.runtime:page:tools"));
        QVERIFY(!host.router.current() ||
                host.router.current()->name() != "test.runtime:page:tools");
        page.reset();
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Disabled);
        QCOMPARE(host.playback.volume(), 30);
        QVERIFY(!host.runtime.context(host.installation()));
        QCOMPARE(host.shell.tasks()->count(), 0);
        QVERIFY(host.runtime.setEnabled(host.installation(), true));
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        QVERIFY(host.runtime.context(host.installation())->activationGeneration() != "1");
    }
    void failureRollsBackAndPendingActivationCancels()
    {
        Host host;
        const auto source = package(host.temporary.path(), R"JS(
export function activate(ctx) {
 ctx.ui.registerTitlebarAction('broken','Never published','settings',()=>{});
 ctx.ui.createTask('broken','Never published');
 ctx.onDispose(()=>{ throw 'cleanup'; });
 throw 'activation error';
}
)JS");
        host.runtime.importDirectory(source);
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Failed);
        QVERIFY(!host.shell.titlebarActions()->activate("test.runtime:titlebar:broken"));
        QCOMPARE(host.shell.tasks()->count(), 0);
        QVERIFY(!host.runtime.data(host.runtime.index(0), PluginRuntime::ErrorRole)
                     .toString()
                     .isEmpty());
        QVERIFY(!host.runtime.safeMode());
        QVERIFY(host.runtime.record(host.installation())
                    .value("failure")
                    .toMap()
                    .value("message")
                    .toString()
                    .contains("activation error"));
    }
    void applicationQuitDrainsPluginIoFixture()
    {
        const auto mode = qEnvironmentVariable("ECHO_PLUGIN_QUIT_PROBE");
        if (mode.isEmpty())
            QSKIP("Subprocess fixture; exercised by applicationQuitDrainsPluginIo");
        Host host;
        PluginRuntime runtime(
            host.engine, host.settings,
            {host.playback, host.theme, host.shell, host.router, host.account, host.api},
            host.temporary.path() + "/exit-plugins");
        runtime.start({package(host.temporary.path(),
                               "import {awaitOperation} from 'echo:plugin-sdk'; export function "
                               "activate(c) { return awaitOperation(c.io.writeValue('ready',1)); }",
                               {"storage"})});
        QTRY_COMPARE(runtime.rowCount(), 1);
        const auto installation =
            runtime.data(runtime.index(0), PluginRuntime::InstallationRole).toString();
        QTRY_COMPARE(runtime.state(installation), PluginRuntime::Active);
        QPointer<EchoPluginContext> context = runtime.context(installation);
        QVERIFY(context);
        const auto connection = QStringLiteral("quit-drain-values");
        auto cleanup = qScopeGuard([&] { QSqlDatabase::removeDatabase(connection); });
        {
            auto database = QSqlDatabase::addDatabase("QSQLITE", connection);
            database.setDatabaseName(runtime.directory() + "/data/" + installation +
                                     "/values.sqlite");
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec("BEGIN EXCLUSIVE"));
            for (int i = 0; i < 32; ++i)
                context->io()->writeValue(QString("quit-%1").arg(i), i);
            int requests = 0;
            bool unlocked = false, drained = false;
            ApplicationShutdown shutdown(*qApp, [&] {
                ++requests;
                runtime.shutdown();
            });
            connect(&runtime, &PluginRuntime::closed, &shutdown, [&] {
                drained = true;
                shutdown.complete();
            });
            // A real locked SQLite worker needs GUI event processing to unlock.
            // Blocking the GUI to join instead of draining cannot pass this case.
            QTimer::singleShot(150, &shutdown, [&] { unlocked = query.exec("COMMIT"); });
            QTimer::singleShot(0, &shutdown, [&] {
                if (mode.startsWith("exit"))
                    QCoreApplication::exit(7);
                else if (mode == "posted")
                    QCoreApplication::postEvent(qApp, new QEvent(QEvent::Quit));
                else
                    QCoreApplication::quit();
                if (mode == "repeated") {
                    QCoreApplication::quit();
                    QTimer::singleShot(1, &shutdown, &QCoreApplication::quit);
                }
                if (mode == "exit-repeated")
                    QTimer::singleShot(1, &shutdown, [] { QCoreApplication::exit(9); });
            });
            const int result = QCoreApplication::exec();
            QCOMPARE(result, mode.startsWith("exit") ? 7 : 0);
            QCOMPARE(requests, 1);
            QVERIFY(unlocked && drained);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(context.isNull());
            QVERIFY(query.prepare("SELECT json FROM plugin_values WHERE key=?"));
            for (int i = 0; i < 32; ++i) {
                query.bindValue(0, QString("quit-%1").arg(i));
                QVERIFY(query.exec());
                QVERIFY(query.next());
                QCOMPARE(QJsonDocument::fromJson(query.value(0).toByteArray())
                             .object()
                             .value("value")
                             .toInt(),
                         i);
            }
            qInfo("Plugin quit probe: drained and preserved all 32 accepted SQL writes");
        }
    }
    void applicationQuitDrainsPluginIo_data()
    {
        QTest::addColumn<QString>("mode");
        for (const auto &mode : {"quit", "posted", "exit", "repeated", "exit-repeated"})
            QTest::newRow(mode) << QString(mode);
    }
    void applicationQuitDrainsPluginIo()
    {
        QFETCH(QString, mode);
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        environment.insert("ECHO_PLUGIN_QUIT_PROBE", mode);
        child.setProcessEnvironment(environment);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(),
                    {"applicationQuitDrainsPluginIoFixture"});
        QVERIFY(child.waitForStarted());
        const bool finished = child.waitForFinished(15000);
        if (!finished) {
            child.kill();
            child.waitForFinished(5000);
        }
        const auto output = child.readAll();
        QVERIFY2(finished, output.constData());
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QVERIFY2(child.exitCode() == 0, output.constData());
        QVERIFY(output.contains("drained and preserved all 32 accepted SQL writes"));
        QVERIFY(!output.contains("Destroyed while thread"));
    }
    void ioDestructionJoinsAcceptedStorageWrites()
    {
        Host host;
        host.runtime.importDirectory(
            package(host.temporary.path(), "export function activate(c) {}", {"storage"}));
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        const auto directory = host.temporary.path() + "/unwinding-values";
        {
            EchoPluginIo io(*host.runtime.context(host.installation()), directory);
            for (int i = 0; i < 32; ++i)
                io.writeValue(QString("unwind-%1").arg(i), i);
        }
        const auto connection = QStringLiteral("unwound-values");
        auto cleanup = qScopeGuard([&] { QSqlDatabase::removeDatabase(connection); });
        {
            auto database = QSqlDatabase::addDatabase("QSQLITE", connection);
            database.setDatabaseName(directory + "/values.sqlite");
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec("SELECT COUNT(*) FROM plugin_values"));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 32);
        }
    }
    void interruptedProcessFixture()
    {
        const auto root = qEnvironmentVariable("ECHO_PLUGIN_PROCESS_ROOT");
        if (root.isEmpty())
            QSKIP("Subprocess fixture; exercised by killedProcessRecovery");
        const auto phase = qEnvironmentVariable("ECHO_PLUGIN_PROCESS_PHASE");
        Host host;
        QSettings preferences(root + "/settings.ini", QSettings::IniFormat);
        PluginRuntime runtime(
            host.engine, preferences,
            {host.playback, host.theme, host.shell, host.router, host.account, host.api},
            root + "/plugins");
        runtime.start({root + "/source"});
        QTRY_COMPARE(runtime.rowCount(), 1);
        const auto installation =
            runtime.data(runtime.index(0), PluginRuntime::InstallationRole).toString();
        QTRY_COMPARE(runtime.state(installation),
                     phase == "activating" ? PluginRuntime::Activating : PluginRuntime::Active);
        if (phase == "deactivating" || phase == "normal") {
            QSignalSpy closed(&runtime, &PluginRuntime::closed);
            runtime.shutdown();
            if (phase == "normal") {
                QTRY_COMPARE(closed.count(), 1);
                write(root + "/ready", "closed");
                return;
            }
            QTRY_COMPARE(runtime.state(installation), PluginRuntime::Deactivating);
        }
        write(root + "/ready", installation.toUtf8());
        // Parent uses QProcess::kill, so destructors and shutdown cannot clean
        // installation data on the test's behalf.
        QEventLoop loop;
        loop.exec();
    }
    void killedProcessRecovery_data()
    {
        QTest::addColumn<QString>("phase");
        QTest::addColumn<bool>("manualSafe");
        QTest::newRow("active-kill-silent") << QString("active") << false;
        QTest::newRow("loading-kill-silent") << QString("activating") << false;
        QTest::newRow("shutdown-kill-silent") << QString("deactivating") << false;
        QTest::newRow("normal-exit-clean") << QString("normal") << false;
        QTest::newRow("explicit-safe-mode-retained") << QString("active") << true;
    }
    void killedProcessRecovery()
    {
        QFETCH(QString, phase);
        QFETCH(bool, manualSafe);
        QTemporaryDir directory;
        const auto root = directory.path();
        const auto source =
            package(root,
                    phase == "activating"
                        ? "export function activate(c) { return new Promise(()=>{}); }"
                        : "import {awaitOperation} from 'echo:plugin-sdk'; export function "
                          "activate(c) { return awaitOperation(c.io.writeValue('kept',41)); }",
                    {"lifecycle", "storage"});
        if (phase == "deactivating") {
            QFile entry(source + "/entry.mjs");
            QVERIFY(entry.open(QIODevice::Append));
            entry.write("\nexport function deactivate(c) { return new Promise(()=>{}); }\n");
        }
        QProcess child;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        environment.insert("ECHO_PLUGIN_PROCESS_ROOT", root);
        environment.insert("ECHO_PLUGIN_PROCESS_PHASE", phase);
        child.setProcessEnvironment(environment);
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(), {"interruptedProcessFixture"});
        QVERIFY(child.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(
            QFileInfo::exists(root + "/ready") || child.state() == QProcess::NotRunning, 10000);
        QVERIFY2(QFileInfo::exists(root + "/ready"), child.readAll().constData());
        if (phase == "normal") {
            QVERIFY(child.waitForFinished(10000) || child.state() == QProcess::NotRunning);
            QCOMPARE(child.exitCode(), 0);
        } else {
            child.kill();
            QVERIFY(child.waitForFinished(5000));
            QCOMPARE(child.exitStatus(), QProcess::CrashExit);
        }
        QSettings preferences(root + "/settings.ini", QSettings::IniFormat);
        const auto catalog = preferences.value("plugins/installations").toList();
        QCOMPARE(catalog.size(), 1);
        const auto installation = catalog.first().toMap().value("installation").toString();
        if (manualSafe)
            preferences.setValue("plugins/safeMode", true);
        Host host;
        {
            PluginRuntime runtime(
                host.engine, preferences,
                {host.playback, host.theme, host.shell, host.router, host.account, host.api},
                root + "/plugins");
            QCOMPARE(runtime.safeMode(), manualSafe);
            QSignalSpy failures(&runtime, &PluginRuntime::pluginFailed);
            runtime.start();
            if (phase == "activating")
                QTRY_COMPARE(runtime.state(installation), PluginRuntime::Activating);
            else
                QTRY_COMPARE(runtime.state(installation),
                             manualSafe ? PluginRuntime::Disabled : PluginRuntime::Active);
            QVERIFY(runtime.record(installation).value("enabled").toBool());
            QVERIFY(runtime.record(installation).value("error").toString().isEmpty());
            QVERIFY(runtime.record(installation).value("failure").toMap().isEmpty());
            QCOMPARE(failures.count(), 0);
            if (phase == "activating") {
                QVERIFY(runtime.context(installation));
                QVERIFY(runtime.setEnabled(installation, false));
                QTRY_COMPARE(runtime.state(installation), PluginRuntime::Disabled);
                QCOMPARE(failures.count(), 0);
            } else if (!manualSafe) {
                auto *operation = runtime.context(installation)->io()->readValue("kept");
                const auto jsRoot = host.engine.newQObject(operation);
                QTRY_VERIFY(operation->finished());
                QCOMPARE(operation->result().toInt(), 41);
            }
            QSignalSpy closed(&runtime, &PluginRuntime::closed);
            runtime.shutdown();
            QTRY_COMPARE_WITH_TIMEOUT(closed.count(), 1, 10000);
        }
    }

    void promiseCanceledBeforeCommit()
    {
        Host host;
        const auto source = package(host.temporary.path(), R"JS(
export function activate(ctx) {
 ctx.ui.registerTitlebarAction('pending','Never committed','settings',()=>{});
 return new Promise(()=>{});
}
)JS");
        host.runtime.importDirectory(source);
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Activating);
        QVERIFY(host.runtime.setEnabled(host.installation(), false));
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Disabled);
        QVERIFY(!host.shell.titlebarActions()->activate("test.runtime:titlebar:pending"));
    }
    void rejectedPromiseAndLifecycleTimeout()
    {
        Host host;
        const auto source =
            package(host.temporary.path(),
                    "export function activate(ctx) { return Promise.reject('rejected'); }\n");
        host.runtime.importDirectory(source);
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Failed);
        QVERIFY(host.runtime.data(host.runtime.index(0), PluginRuntime::ErrorRole)
                    .toString()
                    .contains("rejected"));
        write(source + "/entry.mjs",
              "export function activate(ctx) { return new Promise(()=>{}); }\n");
        QVERIFY(host.runtime.reload(host.installation()));
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Activating);
        QTRY_COMPARE_WITH_TIMEOUT(host.runtime.state(host.installation()), PluginRuntime::Failed,
                                  7000);
        QVERIFY(host.runtime.data(host.runtime.index(0), PluginRuntime::ErrorRole)
                    .toString()
                    .contains("timed out"));
    }
    void reloadUsesNewCodeUrlAndIdentity()
    {
        Host host;
        const auto source = package(
            host.temporary.path(), "export function activate(ctx) { ctx.player.setVolume(20); }\n");
        host.runtime.importDirectory(source);
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        QTRY_COMPARE(host.playback.volume(), 20);
        const auto identity = host.installation();
        const auto firstRoot = host.runtime.context(identity)->package().root;
        write(source + "/entry.mjs",
              "export function activate(ctx) { ctx.player.setVolume(70); }\n");
        QVERIFY(host.runtime.reload(identity));
        QTRY_COMPARE(host.runtime.state(identity), PluginRuntime::Active);
        QTRY_COMPARE(host.playback.volume(), 70);
        QCOMPARE(host.installation(), identity);
        QVERIFY(host.runtime.context(identity)->package().root != firstRoot);
    }
    void actualHttpAndCancellation()
    {
        Host host;
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [socket] {
                socket->readAll();
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\nreal");
                socket->disconnectFromHost();
            });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        host.runtime.importDirectory(package(host.temporary.path(),
                                             "export function activate(ctx) {}", {"net.http"},
                                             {"unrestrictedNetwork"}));
        QTRY_COMPARE(host.runtime.state(host.installation()), PluginRuntime::Active);
        auto *io = host.runtime.context(host.installation())->io();
        const auto url = QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) + "/test");
        auto *request = io->get(url);
        QTRY_COMPARE(request->state(), EchoPluginOperation::Succeeded);
        QCOMPARE(request->result().toMap().value("status").toInt(), 200);
        QCOMPARE(request->result().toMap().value("body").toByteArray(), QByteArray("real"));
        auto *canceled = io->get(url);
        QSignalSpy completed(canceled, &EchoPluginOperation::completed);
        canceled->cancel();
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(canceled->state(), EchoPluginOperation::Canceled);
    }
    void persistedInstallAndSafeMode()
    {
        QTemporaryDir directory;
        QSettings settings(directory.path() + "/settings.ini", QSettings::IniFormat);
        ApiClient api(DeviceIdentity::temporary());
        AccountService account(api);
        PlaybackService playback(api, settings, nullptr, directory.path() + "/playback.sqlite");
        ThemeStore theme(settings);
        RouteController router;
        ShellController shell(settings, router);
        QQmlEngine engine;
        const auto root = directory.path() + "/plugins";
        const auto source = package(root, "export function activate(ctx) {}\n");
        QString installation;
        {
            PluginRuntime runtime(engine, settings, {playback, theme, shell, router, account, api},
                                  root);
            runtime.start({source});
            QTRY_COMPARE(runtime.rowCount(), 1);
            installation =
                runtime.data(runtime.index(0), PluginRuntime::InstallationRole).toString();
            QTRY_COMPARE(runtime.state(installation), PluginRuntime::Active);
            QSignalSpy closed(&runtime, &PluginRuntime::closed);
            runtime.shutdown();
            QTRY_COMPARE(closed.count(), 1);
        }
        {
            PluginRuntime runtime(engine, settings, {playback, theme, shell, router, account, api},
                                  root);
            runtime.start();
            QTRY_VERIFY(!runtime.loading());
            QCOMPARE(runtime.rowCount(), 1);
            QCOMPARE(
                runtime.record(installation).value("origin").toMap().value("directory").toString(),
                QFileInfo(source).canonicalFilePath());
            QCOMPARE(runtime.context(installation)->activationGeneration(), QString("1"));
            QSignalSpy closed(&runtime, &PluginRuntime::closed);
            runtime.shutdown();
            QTRY_COMPARE(closed.count(), 1);
        }
        QDir(source).removeRecursively();
        {
            PluginRuntime runtime(engine, settings, {playback, theme, shell, router, account, api},
                                  root);
            runtime.start();
            QTRY_COMPARE(runtime.state(installation), PluginRuntime::Active);
            QSignalSpy closed(&runtime, &PluginRuntime::closed);
            runtime.shutdown();
            QTRY_COMPARE(closed.count(), 1);
        }
        settings.setValue("plugins/safeMode", true);
        PluginRuntime safe(engine, settings, {playback, theme, shell, router, account, api}, root);
        QVERIFY(safe.safeMode());
        safe.start();
        QTRY_COMPARE(safe.state(installation), PluginRuntime::Disabled);
        QVERIFY(!safe.context(installation));
        QVERIFY(safe.data(safe.index(0), PluginRuntime::EnabledRole).toBool());
        QSignalSpy closed(&safe, &PluginRuntime::closed);
        safe.shutdown();
        QTRY_COMPARE(closed.count(), 1);
        playback.shutdown();
        api.shutdown();
    }
};
QTEST_MAIN(PluginRuntimeTest)
#include "PluginRuntimeTest.moc"
