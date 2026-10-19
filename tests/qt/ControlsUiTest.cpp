#include "EchoIconSource.h"
#include "EchoBackdropRoot.h"
#include "EchoSelectModel.h"
#include <QAbstractItemModelTester>
#include <QStringListModel>
#include <QStandardItemModel>
#include "NativeWindow.h"
#include "ThemeStore.h"
#include "UiTypography.h"
#include <QDir>
#include <QFontDatabase>
#include <QPainterPath>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextLayout>
#include <QtTest>
#include <QTimer>
#include <functional>
#include <ctime>

using namespace EchoMusic;
namespace
{
QQuickItem *findVisual(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *found = findVisual(child, name))
            return found;
    return nullptr;
}
struct ControlsSession
{
    QTemporaryDir directory;
    QSettings settings{directory.filePath("settings.ini"), QSettings::IniFormat};
    ThemeStore theme{settings};
    QQmlEngine engine;
    QScopedPointer<QObject> root;
    QQuickWindow *window = nullptr;
    bool create(const QString &harness = QStringLiteral("ControlsHarness.qml"))
    {
        theme.attachToEngine(engine);
        QQmlComponent component(&engine, QUrl("qrc:/qt/qml/EchoMusic/ControlsTests/" + harness));
        root.reset(component.create());
        if (!root) {
            qWarning().noquote() << component.errorString();
            return false;
        }
        window = qobject_cast<QQuickWindow *>(root.data());
        return window && Test::prepareNativeInput(window);
    }
    QQuickItem *item(const QString &name)
    {
        return findVisual(window->contentItem(), name);
    }
    ~ControlsSession()
    {
        if (window)
            window->close();
    }
};
} // namespace
class ControlsUiTest : public QObject
{
    Q_OBJECT
private slots:
    void badgeIconAndCountUseSameInkCenter()
    {
        ControlsSession session;
        session.theme.attachToEngine(session.engine);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QQmlComponent component(&session.engine);
        component.setData(R"(
import QtQuick
import EchoMusic.Controls
import EchoMusic.Theme
Window {
    id: window
    visible: true; width: 420; height: 360
    property real sampleScale: 1
    Rectangle { anchors.fill: parent; color: Theme.main }
    Column {
        x: 24; y: 24; spacing: 12
        scale: window.sampleScale; transformOrigin: Item.TopLeft
        Repeater {
            model: ["4344", "9", "1.2w", "中文123", "Artist"]
            delegate: EchoBadge {
                required property int index
                required property string modelData
                objectName: "likeBadge-" + index
                count: modelData; glyph: "thumb-up"; pill: true
                height: 28.5; font: Theme.font(11, Theme.semiboldWeight)
                horizontalPadding: 10; foreground: Theme.accentText
                tone: EchoBadge.Muted
            }
        }
    }
})",
                          QUrl("qrc:/badge-icon-ink.qml"));
        session.root.reset(component.create());
        QVERIFY2(session.root, qPrintable(component.errorString()));
        session.window = qobject_cast<QQuickWindow *>(session.root.data());
        QVERIFY(session.window && Test::prepareNativeInput(session.window));
        if (session.window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
            QSKIP("Icon/text ink pixels require native RHI");
        const auto capture = qEnvironmentVariable("ECHO_BADGE_ICON_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        QStringList failures;
        for (bool dark : {false, true})
            for (qreal scale : {1., 1.25}) {
                session.theme.setMode(dark ? ThemeStore::Dark : ThemeStore::Light);
                session.root->setProperty("sampleScale", scale);
                for (int i = 0; i < 5; ++i) {
                    auto *glyph =
                        findVisual(session.item("likeBadge-" + QString::number(i)), "buttonGlyph");
                    QVERIFY(glyph);
                    QTRY_COMPARE(glyph->property("status").toInt(), 1);
                }
                QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                session.window->update();
                QTRY_VERIFY(frames.size() > 0);
                const auto image = session.window->grabWindow();
                QVERIFY(!image.isNull());
                const qreal ratio = image.width() / qreal(session.window->width());
                if (!capture.isEmpty())
                    QVERIFY(image.save(
                        capture +
                        QString("/badge-%1-%2.png").arg(dark ? "dark" : "light").arg(scale)));
                for (int i = 0; i < 5; ++i) {
                    auto *badge = session.item("likeBadge-" + QString::number(i));
                    auto *glyph = findVisual(badge, "buttonGlyph");
                    auto *label = findVisual(badge, "buttonLabel");
                    QVERIFY(glyph && label);
                    const auto color = label->property("color").value<QColor>();
                    const auto inkCenter = [&](QQuickItem *item) {
                        const auto bounds =
                            item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
                        const auto pixels = QRectF(bounds.topLeft() * ratio, bounds.size() * ratio)
                                                .toAlignedRect()
                                                .intersected(image.rect());
                        int top = image.height(), bottom = -1;
                        for (int y = pixels.top(); y <= pixels.bottom(); ++y)
                            for (int x = pixels.left(); x <= pixels.right(); ++x) {
                                const auto p = image.pixelColor(x, y);
                                if (qAbs(p.red() - color.red()) + qAbs(p.green() - color.green()) +
                                        qAbs(p.blue() - color.blue()) <
                                    60) {
                                    top = std::min(top, y);
                                    bottom = std::max(bottom, y);
                                }
                            }
                        return bottom < top ? -1. : (top + bottom + 1) / 2.;
                    };
                    const auto iconInk = inkCenter(glyph), textInk = inkCenter(label);
                    QVERIFY(iconInk >= 0 && textInk >= 0);
                    const auto delta = iconInk - textInk;
                    qInfo() << "badge_ink" << dark << scale << badge->property("displayText")
                            << "icon_minus_text_pixels" << delta;
                    if (qAbs(delta) > 1.)
                        failures.append(QString("%1 scale %2 dark %3: %4px")
                                            .arg(badge->property("displayText").toString())
                                            .arg(scale)
                                            .arg(dark)
                                            .arg(delta));
                }
            }
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join('\n')));
        QCOMPARE(warnings.size(), 0);
    }
    void scrollContentCannotPaintOverRoundedCornersOrBorder()
    {
        ControlsSession session;
        QVERIFY(session.create());
        if (session.window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
            QSKIP("Rounded content pixels require native RHI");
        session.window->resize(1100, 800);
        QTRY_COMPARE(session.window->height(), 800);
        session.theme.setMotionEnabled(false);
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            import EchoMusic.Theme
            Item {
                id: host
                property real panelScale: 1
                property bool padded: false
                property alias drawer: drawerOwner.item
                property alias dialog: dialogOwner.item
                function openDrawer(): void { drawerOwner.open(); }
                function openDialog(): void { dialogOwner.open(); }
                Component {
                    id: scrollingBody
                    Flickable {
                        id: scrollingFixture
                        property bool painted: true
                        contentWidth: width; contentHeight: 2000; clip: true
                        Rectangle { width: parent.width; height: 2000; color: scrollingFixture.painted ? Qt.rgba(147 / 255, 147 / 255, 147 / 255, 1) : "transparent" }
                    }
                }
                EchoPopupLoader {
                    id: drawerOwner; hostItem: host
                    sourceComponent: EchoDrawer {
                        uiScale: host.panelScale
                        contentItem: Loader { sourceComponent: scrollingBody }
                    }
                }
                EchoPopupLoader {
                    id: dialogOwner; hostItem: host
                    sourceComponent: EchoDialog {
                        uiScale: host.panelScale; preferredWidth: 420
                        height: 400; padding: host.padded ? 24 : 0
                        rightPadding: padding
                        contentItem: Loader { sourceComponent: scrollingBody }
                    }
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(host);
        host->setParentItem(session.window->contentItem());
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const qreal scale : {1., 1.25}) {
                host->setProperty("panelScale", scale);
                for (const bool frost : {false, true}) {
                    session.theme.setFloatingFrosted(frost);
                    for (const auto *kind : {"drawer", "dialog"}) {
                        QVERIFY(QMetaObject::invokeMethod(
                            host, QByteArray(kind) == "drawer" ? "openDrawer" : "openDialog"));
                        QPointer<QObject> popup = host->property(kind).value<QObject *>();
                        QVERIFY(popup);
                        QTRY_VERIFY(popup->property("opened").toBool());
                        QPointer<QQuickItem> surface =
                            popup->property("background").value<QQuickItem *>();
                        auto *content = popup->property("contentItem").value<QQuickItem *>();
                        QVERIFY(surface && content);
                        QPointer<QQuickItem> mask;
                        QTRY_VERIFY(
                            popup && surface &&
                            (mask = popup->findChild<QQuickItem *>("popupContentRoundedMask")));
                        auto *view = content->property("item").value<QQuickItem *>();
                        QVERIFY(view);
                        QVERIFY(QQmlProperty(content, "layer.enabled").read().toBool());
                        QVERIFY(QQmlProperty(content, "layer.effect").read().value<QObject *>());
                        view->setProperty("contentY", 2000. - view->height());
                        view->setProperty("painted", false);
                        QTest::qWait(70);
                        const auto empty = session.window->grabWindow();
                        view->setProperty("painted", true);
                        QTest::qWait(70);
                        const auto filled = session.window->grabWindow();
                        QVERIFY(popup && surface);
                        QVERIFY(!empty.isNull() && !filled.isNull());
                        const auto pixel = [&](const QImage &image, QPointF p) {
                            p = surface->mapToScene(p) * image.devicePixelRatio();
                            return image.pixelColor(qFloor(p.x()), qFloor(p.y()));
                        };
                        for (const auto &p : {QPointF(1, 1), QPointF(surface->width() - 2, 1),
                                              QPointF(1, surface->height() - 2),
                                              QPointF(surface->width() - 2, surface->height() - 2),
                                              QPointF(.1, surface->height() / 2)}) {
                            const auto a = pixel(empty, p), b = pixel(filled, p);
                            QVERIFY2(
                                qAbs(a.red() - b.red()) < 4 && qAbs(a.green() - b.green()) < 4 &&
                                    qAbs(a.blue() - b.blue()) < 4,
                                qPrintable(QString("%1 corner/border changed at %2,%3: %4 -> %5")
                                               .arg(kind)
                                               .arg(p.x())
                                               .arg(p.y())
                                               .arg(a.name(), b.name())));
                        }
                        const auto inside =
                            pixel(filled, {surface->width() / 2, surface->height() - 20});
                        QVERIFY(qAbs(inside.red() - 147) < 4);
                        const auto captures = qEnvironmentVariable("ECHO_PANEL_CAPTURES");
                        if (!captures.isEmpty()) {
                            QDir().mkpath(captures);
                            QVERIFY(
                                filled.save(captures + QString("/scroll-corners-%1-%2-%3-%4.png")
                                                           .arg(kind)
                                                           .arg(int(mode))
                                                           .arg(scale)
                                                           .arg(frost)));
                        }
                        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                        QTRY_VERIFY(popup.isNull() && mask.isNull());
                    }
                }
            }
        }
        host->setProperty("padded", true);
        QVERIFY(QMetaObject::invokeMethod(host, "openDialog"));
        QPointer<QObject> popup = host->property("dialog").value<QObject *>();
        QVERIFY(popup);
        QTRY_VERIFY(popup->property("opened").toBool());
        QVERIFY(!popup->findChild<QQuickItem *>("popupContentRoundedMask"));
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        QTRY_VERIFY(popup.isNull());
    }
    void coverCropAndRoundedPixels()
    {
        ControlsSession session;
        QVERIFY(session.create());
        if (session.window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
            QSKIP("Cover pixels require native RHI");
        QImage art(240, 120, QImage::Format_RGB32);
        art.fill(Qt::green);
        for (int y = 0; y < 120; ++y)
            for (int x = 0; x < 240; ++x)
                if (x < 60 || x >= 180)
                    art.setPixelColor(x, y, x < 60 ? Qt::red : Qt::blue);
        const auto path = session.directory.filePath("wide-cover.png");
        QVERIFY(art.save(path));
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            Item {
                width: 720; height: 290
                property real zoom: 1
                Rectangle { anchors.fill: parent; color: "black" }
                EchoCover {
                    objectName: "coverProbe"
                    x: 400; y: 60; width: 100; height: 100
                    cornerRadius: circular ? width / 2 : 20; placeholderColor: "transparent"
                    scale: parent.zoom; transformOrigin: Item.TopLeft
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(object.data());
        host->setParentItem(session.window->contentItem());
        auto *cover = findVisual(host, "coverProbe");
        QVERIFY(cover);
        cover->setProperty("source", QUrl::fromLocalFile(path));
        QTRY_COMPARE(cover->property("status").toInt(), int(1));
        for (const qreal scale : {1., 1.25}) {
            host->setProperty("zoom", scale);
            QTest::qWait(80);
            const auto frame = session.window->grabWindow();
            const qreal ratio = frame.devicePixelRatio();
            const auto pixel = [&](qreal x, qreal y) {
                const auto p = cover->mapToScene({x, y}) * ratio;
                return frame.pixelColor(qRound(p.x()), qRound(p.y()));
            };
            QCOMPARE(pixel(1, 1), QColor(Qt::black));
            QVERIFY2(
                pixel(5, 50).green() > 240 && pixel(5, 50).red() < 10,
                "MultiEffect must preserve the image's aspect crop, not stretch its raw texture");
            int partial = 0;
            const auto origin = cover->mapToScene({0, 0}) * ratio;
            for (int y = 0; y < qCeil(20 * scale * ratio); ++y)
                for (int x = 0; x < qCeil(20 * scale * ratio); ++x) {
                    const auto p = frame.pixelColor(qRound(origin.x()) + x, qRound(origin.y()) + y);
                    partial += p.green() > 20 && p.green() < 235;
                }
            qInfo() << "Cover partial edge pixels" << scale << partial;
            QVERIFY2(partial > 12, "Cover corners must retain antialias coverage");
            const auto captures = qEnvironmentVariable("ECHO_COVER_CAPTURE_DIR");
            if (!captures.isEmpty())
                QVERIFY(frame.save(captures + QString("/cover-%1.png").arg(scale)));
        }
        QPointer<QQuickItem> mask = findVisual(cover, "coverRoundedMask");
        QVERIFY(mask);
        cover->setProperty("circular", true);
        QTest::qWait(80);
        auto frame = session.window->grabWindow();
        const auto origin = cover->mapToScene({5, 5}) * frame.devicePixelRatio();
        QCOMPARE(frame.pixelColor(qRound(origin.x()), qRound(origin.y())), QColor(Qt::black));
        cover->setProperty("resourceActive", false);
        QTRY_VERIFY(mask.isNull());
        QTRY_COMPARE(cover->property("status").toInt(), int(0));
        cover->setProperty("resourceActive", true);
        QTRY_COMPARE(cover->property("status").toInt(), int(1));
        QTRY_VERIFY(findVisual(cover, "coverRoundedMask"));
    }
    void frostedCornersRetainAntialiasCoverage()
    {
        ControlsSession session;
        QVERIFY(session.create());
        if (session.window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
            QSKIP("Corner coverage needs native RHI");
        session.window->setColor(Qt::black);
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            Item {
                id: host
                width: 720; height: 290
                property real panelScale: 1
                Rectangle { anchors.fill: parent; color: "black" }
                EchoBackdropRoot {
                    visible: false
                    anchors.fill: parent
                    Rectangle { anchors.fill: parent; color: "white" }
                }
                EchoFloatingSurface {
                    objectName: "coverageProbe"
                    x: 400; y: 80; width: 100; height: 100; radius: 20
                    scale: host.panelScale; transformOrigin: Item.TopLeft
                    color: "transparent"; border.width: 0
                    shadowEnabled: false; frostedEnabled: true
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(host);
        host->setParentItem(session.window->contentItem());
        auto *surface = findVisual(host, "coverageProbe");
        QVERIFY(surface);
        QTRY_VERIFY(surface->property("hasBackdrop").toBool());
        for (const qreal scale : {1., 1.25}) {
            host->setProperty("panelScale", scale);
            QTest::qWait(80);
            const auto frame = session.window->grabWindow();
            QVERIFY(!frame.isNull());
            const auto origin = surface->mapToScene({0, 0});
            const auto ratio = frame.devicePixelRatio();
            const int extent = qCeil(20 * scale * ratio);
            int partial = 0;
            for (int y = 0; y < extent; ++y)
                for (int x = 0; x < extent; ++x) {
                    const auto pixel = frame.pixelColor(qRound(origin.x() * ratio) + x,
                                                        qRound(origin.y() * ratio) + y);
                    partial += pixel.red() > 20 && pixel.red() < 235;
                }
            qInfo() << "Partial corner coverage pixels:" << scale << partial;
            QVERIFY2(partial > 12, "Frosted masks must retain a smooth, partially covered edge");
        }
    }
    void floatingPanelEdgesAndMenuHeight()
    {
        ControlsSession session;
        QVERIFY(session.create());
        session.window->resize(1100, 800);
        session.theme.setMotionEnabled(false);
        qInfo() << "Panel graphics API:" << session.window->rendererInterface()->graphicsApi();
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            import EchoMusic.Theme
            Item {
                id: host
                property real panelScale: 1
                property alias dialog: dialogOwner.item
                property alias drawer: drawerOwner.item
                property alias menu: menuOwner.item
                function showDialog(): void { dialogOwner.open(); }
                function showDrawer(): void { drawerOwner.open(); }
                function showMenu(): void { menuOwner.popup(host, 100, 100); }
                EchoBackdropRoot {
                    anchors.fill: parent
                    Rectangle { anchors.fill: parent; color: Theme.main }
                }
                EchoPopupLoader {
                    id: dialogOwner
                    hostItem: host
                    sourceComponent: EchoDialog {
                        uiScale: host.panelScale
                        heading: "对话框 Dialog"
                        body: Component { Item { implicitHeight: 180 } }
                    }
                }
                EchoPopupLoader {
                    id: drawerOwner
                    hostItem: host
                    sourceComponent: EchoDrawer {
                        uiScale: host.panelScale
                        contentItem: EchoDrawerHeader { heading: "抽屉 Drawer" }
                    }
                }
                EchoPopupLoader {
                    id: menuOwner
                    hostItem: host
                    sourceComponent: EchoMenu {
                        EchoMenuEntry { text: "立即播放" }
                        EchoMenuEntry { text: "下一首播放" }
                        EchoMenuEntry { text: "排队候播" }
                        EchoMenuSeparator {}
                        EchoMenuEntry { text: "添加到" }
                        EchoMenuSeparator { visible: false }
                        EchoMenuEntry { visible: false; text: "隐藏操作" }
                        EchoMenuSeparator {}
                        EchoMenuEntry { text: "分享" }
                        EchoMenuEntry { text: "复制信息" }
                    }
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(host);
        host->setParentItem(session.window->contentItem());
        host->setSize(session.window->size());
        const auto captures = qEnvironmentVariable("ECHO_PANEL_CAPTURES");
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const qreal scale : {1., 1.25}) {
                host->setProperty("panelScale", scale);
                for (const bool frosted : {false, true}) {
                    session.theme.setFloatingFrosted(frosted);
                    for (const auto *name : {"dialog", "drawer"}) {
                        QVERIFY(!host->property(name).value<QObject *>());
                        QVERIFY(QMetaObject::invokeMethod(
                            host, QByteArray(name) == "dialog" ? "showDialog" : "showDrawer"));
                        QPointer<QObject> popup = host->property(name).value<QObject *>();
                        QVERIFY(popup);
                        QTRY_VERIFY(popup->property("opened").toBool());
                        QTest::qWait(60);
                        qInfo() << name << scale << frosted
                                << popup->property("background")
                                       .value<QQuickItem *>()
                                       ->property("hasBackdrop");
                        if (!captures.isEmpty()) {
                            QVERIFY(QDir().mkpath(captures));
                            const auto path = QString("/%1-%2-%3-%4.png")
                                                  .arg(name)
                                                  .arg(int(mode))
                                                  .arg(scale)
                                                  .arg(frosted);
                            QVERIFY(session.window->grabWindow().save(captures + path));
                        }
                        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                        QTRY_VERIFY(popup.isNull());
                    }
                }
            }
        }
        QVERIFY(QMetaObject::invokeMethod(host, "showMenu"));
        QPointer<QObject> menu = host->property("menu").value<QObject *>();
        QVERIFY(menu);
        QTRY_VERIFY(menu->property("opened").toBool());
        auto *list = menu->property("contentItem").value<QQuickItem *>();
        QVERIFY(list);
        QTest::qWait(100);
        qInfo() << "Menu content / viewport:" << list->property("contentHeight") << list->height();
        QVERIFY2(list->property("contentHeight").toReal() <= list->height() + .5,
                 "A menu which fits the window must not have a tiny scroll range");
        QVERIFY(QMetaObject::invokeMethod(menu, "close"));
        QTRY_VERIFY(menu.isNull());
        session.window->resize(1100, 300);
        QVERIFY(QMetaObject::invokeMethod(host, "showMenu"));
        menu = host->property("menu").value<QObject *>();
        QVERIFY(menu);
        QTRY_VERIFY(menu->property("opened").toBool());
        list = menu->property("contentItem").value<QQuickItem *>();
        QVERIFY(list);
        QTest::qWait(100);
        QVERIFY(list->property("contentHeight").toReal() > list->height());
        QVERIFY(list->property("interactive").toBool());
        QVERIFY(QMetaObject::invokeMethod(menu, "close"));
        QTRY_VERIFY(menu.isNull());
    }
    void clearActionIsOneUserEditButProgrammaticClearIsNot()
    {
        ControlsSession session;
        QVERIFY(session.create());
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            Item {
                width: 320; height: 100
                EchoTextField {
                    objectName: "clearInputUnderTest"
                    x: 20; y: 20; width: 260
                    clearable: true
                    property string draft: ""
                    onTextEdited: draft = text
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(host);
        host->setParentItem(session.window->contentItem());
        auto *field = findVisual(host, "clearInputUnderTest");
        QVERIFY(field);
        auto *clear = findVisual(field, "clearTextAction");
        QVERIFY(clear);
        QSignalSpy edited(field, SIGNAL(textEdited()));
        QSignalSpy cleared(field, SIGNAL(cleared()));
        for (const int echoMode : {0, 2}) {
            field->setProperty("echoMode", echoMode);
            field->setProperty("text", QStringLiteral("中文🙂Alpha"));
            QCOMPARE(edited.size(), 0);
            QCOMPARE(cleared.size(), 0);
            field->setProperty("readOnly", true);
            QVERIFY(!clear->isVisible());
            field->setProperty("readOnly", false);
            field->setEnabled(false);
            QVERIFY(!clear->isVisible());
            field->setEnabled(true);
            QTRY_VERIFY(clear->isVisible());
            QVERIFY(Test::prepareNativeInput(session.window));
            const auto point =
                clear->mapToScene({clear->width() / 2, clear->height() / 2}).toPoint();
            Test::moveNativePointer(session.window, point);
            QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point);
            QTRY_COMPARE(field->property("text").toString(), QString());
            QCOMPARE(field->property("draft").toString(), QString());
            QCOMPARE(edited.size(), 1);
            QCOMPARE(cleared.size(), 1);
            QVERIFY(field->hasActiveFocus());
            QVERIFY(!clear->isVisible());
            for (const auto key : QByteArray("AB"))
                QTest::keyClick(session.window, key);
            QTest::keyClick(session.window, Qt::Key_Backspace);
            QCOMPARE(field->property("text").toString(), QString("A"));
            QCOMPARE(field->property("draft").toString(), QString("A"));
            QCOMPARE(edited.size(), 4);
            edited.clear();
            cleared.clear();
            // Code-driven resets retain Qt's native, non-user-edit semantics.
            QVERIFY(QMetaObject::invokeMethod(field, "clear"));
            QCOMPARE(field->property("text").toString(), QString());
            QCOMPARE(field->property("draft").toString(), QString("A"));
            QCOMPARE(edited.size(), 0);
            QCOMPARE(cleared.size(), 0);
        }
    }
    void colorPresetsKeepVisibleEdgesInBothThemes()
    {
        ControlsSession session;
        QVERIFY(session.create());
        if (session.window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software)
            QSKIP("Color swatch pixels require native RHI");
        session.window->resize(800, 700);
        session.theme.setMotionEnabled(false);
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            Item {
                property alias popup: picker
                EchoColorPickerDialog {
                    id: picker
                    title: "选择歌词颜色"
                    value: "#ffffff"
                    presets: ["#ffffff", "#fafafa", "#000000", "#31cfa1"]
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(host);
        host->setParentItem(session.window->contentItem());
        auto *popup = host->property("popup").value<QObject *>();
        QVERIFY(popup);
        QSignalSpy confirmed(popup, SIGNAL(confirmed(QString)));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const qreal scale : {1., 1.25}) {
                popup->setProperty("uiScale", scale);
                QVERIFY(QMetaObject::invokeMethod(popup, "open"));
                QTRY_VERIFY(popup->property("opened").toBool());
                QVERIFY(Test::prepareNativeInput(session.window));
                Test::moveNativePointer(session.window, QPoint(10, 10));
                auto *white = session.item("colorPreset-0");
                auto *nearWhite = session.item("colorPreset-1");
                auto *black = session.item("colorPreset-2");
                auto *mint = session.item("colorPreset-3");
                QVERIFY(white && nearWhite && black && mint);
                const auto snapshot = [&] {
                    session.window->update();
                    QTest::qWait(80);
                    return session.window->grabWindow();
                };
                const auto selected = snapshot();
                const auto checkVisibleEdge = [&](const QImage &image, QQuickItem *swatch) {
                    const auto dpr = image.width() / qreal(session.window->width());
                    const auto sample = [&](qreal x) {
                        const auto point = swatch->mapToScene({x, swatch->height() / 2}) * dpr;
                        return image.pixelColor(qRound(point.x()), qRound(point.y()));
                    };
                    return qAbs(qGray(sample(.35).rgb()) - qGray(sample(-2).rgb())) > 12;
                };
                QVERIFY(checkVisibleEdge(selected, white));
                QVERIFY(checkVisibleEdge(selected, nearWhite));
                QVERIFY(checkVisibleEdge(selected, black));
                const auto dpr = selected.width() / qreal(session.window->width());
                const auto center =
                    white->mapToScene({white->width() / 2, white->height() / 2}) * dpr;
                int checkInk = 0;
                const int extent = qCeil(8 * scale * dpr);
                for (int y = qRound(center.y()) - extent; y < qRound(center.y()) + extent; ++y)
                    for (int x = qRound(center.x()) - extent; x < qRound(center.x()) + extent; ++x)
                        checkInk += qGray(selected.pixel(x, y)) < 90;
                QVERIFY(checkInk > 8);
                const auto position =
                    mint->mapToScene({mint->width() / 2, mint->height() / 2}).toPoint();
                Test::moveNativePointer(session.window, position);
                QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, position);
                QTRY_COMPARE(popup->property("draft").toString(), QString("#31cfa1"));
                const auto unselected = snapshot();
                QVERIFY(checkVisibleEdge(unselected, white));
                const auto captures = qEnvironmentVariable("ECHO_SELECT_COLOR_CAPTURES");
                if (!captures.isEmpty()) {
                    QDir().mkpath(captures);
                    const auto name = QString("/swatches-%1-%2").arg(int(mode)).arg(scale);
                    QVERIFY(selected.save(captures + name + "-selected.png"));
                    QVERIFY(unselected.save(captures + name + "-unselected.png"));
                }
                QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                QTRY_VERIFY(!popup->property("visible").toBool());
                QCOMPARE(popup->property("value").toString(), QString("#ffffff"));
                QCOMPARE(confirmed.size(), 0);
            }
        }
    }
    void boundedListAutoHeightSurvivesReopenAndVariableRows()
    {
        ControlsSession session;
        QVERIFY(session.create());
        session.window->resize(900, 800);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            import EchoMusic.Theme
            Item {
                id: host
                property var rows: [42, 87, 53]
                property string rowText: "中文 Latin fallback"
                property real footerHeight: 0
                property alias popup: dialog
                EchoDialog {
                    id: dialog
                    heading: "变高列表"
                    preferredWidth: 420
                    body: EchoBoundedListView {
                        id: list
                        objectName: "boundedTestList"
                        maximumViewportHeight: 280
                        model: dialog.visible ? host.rows : null
                        clip: true
                        reuseItems: true
                        cacheBuffer: 0
                        spacing: 7
                        topMargin: 2
                        bottomMargin: 2
                        footer: Rectangle {
                            width: list.width
                            height: host.footerHeight
                            color: Theme.floating.mutedSurface
                        }
                        delegate: Rectangle {
                            required property var modelData
                            objectName: "boundedRow"
                            width: list.width
                            height: Math.max(modelData, label.implicitHeight + 16)
                            color: Theme.floating.mutedSurface
                            Text {
                                id: label
                                x: 8; y: 8; width: parent.width - 16
                                text: host.rowText
                                font: Theme.font(12, Theme.regularWeight)
                                color: Theme.floating.text
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(host);
        host->setParentItem(session.window->contentItem());
        auto *popup = host->property("popup").value<QObject *>();
        QQuickItem *view = nullptr;
        QVERIFY(popup);
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const auto scale : {1., 1.25}) {
                popup->setProperty("uiScale", scale);
                qreal height = 0;
                for (int cycle = 0; cycle < 4; ++cycle) {
                    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
                    QTRY_VERIFY(popup->property("opened").toBool());
                    QTRY_VERIFY(
                        (view = findVisual(session.window->contentItem(), "boundedTestList")));
                    QTRY_COMPARE(view->property("count").toInt(), 3);
                    QTRY_COMPARE(view->implicitHeight(), 200.);
                    QTRY_COMPARE(view->height(), 200.);
                    if (!cycle)
                        height = popup->property("height").toReal();
                    QCOMPARE(popup->property("height").toReal(), height);
                    QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                    QTRY_VERIFY(!popup->property("visible").toBool());
                }
            }
        }
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("opened").toBool());
        // Same row count with different geometry must invalidate the previous extent.
        host->setProperty("rows", QVariantList{42, 120, 60});
        QTRY_COMPARE(view->implicitHeight(), 240.);
        host->setProperty("rows", QVariantList{42, 87, 53, 50});
        QTRY_COMPARE(view->implicitHeight(), 257.);
        host->setProperty("rows", QVariantList{});
        QTRY_COMPARE(view->implicitHeight(), 0.);
        host->setProperty("footerHeight", 44.);
        QTRY_COMPARE(view->implicitHeight(), 44.);
        host->setProperty("rows", QVariantList{42, 87, 53});
        QTRY_COMPARE(view->implicitHeight(), 244.);
        host->setProperty("footerHeight", 100.);
        QTRY_COMPARE(view->implicitHeight(), 280.);
        host->setProperty("footerHeight", 0.);
        QTRY_COMPARE(view->implicitHeight(), 200.);
        // A real Text reflows at narrow width and later returns to its original size.
        host->setProperty("rowText", QStringLiteral("中文 Latin 字体回退 ").repeated(25));
        popup->setProperty("preferredWidth", 240.);
        QTRY_COMPARE(view->implicitHeight(), 280.);
        popup->setProperty("preferredWidth", 420.);
        host->setProperty("rowText", QStringLiteral("中文 Latin fallback"));
        QTRY_COMPARE(view->implicitHeight(), 200.);
        QVariantList many;
        for (int i = 0; i < 1000; ++i)
            many.push_back(35 + (i % 5) * 14);
        host->setProperty("rows", many);
        QTRY_COMPARE(view->property("count").toInt(), 1000);
        QTRY_COMPARE(view->implicitHeight(), 280.);
        auto mounted = [&] {
            int count = 0;
            const auto items = view->property("contentItem").value<QQuickItem *>()->childItems();
            for (auto *item : items)
                count += item->objectName() == "boundedRow";
            return count;
        };
        QVERIFY(mounted() > 0 && mounted() < 30);
        view->setProperty("contentY", view->property("contentHeight").toReal() - view->height());
        QTest::qWait(100);
        QCOMPARE(view->implicitHeight(), 280.);
        QVERIFY(mounted() < 30);
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        QTRY_VERIFY(!popup->property("visible").toBool());
        host->setProperty("rows", QVariantList{42, 87, 53});
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("opened").toBool());
        QTRY_COMPARE(view->height(), 200.);
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(warnings.size(), 0);
    }

    void progressClimaxTicksAndWaitPreview()
    {
        ControlsSession session;
        QVERIFY(session.create("ProgressHarness.qml"));
        Test::moveNativePointer(session.window, {700, 140});
        auto *slider = session.item("previewProgress");
        QVERIFY(slider);
        QStandardItemModel marks;
        marks.setItemRoleNames({{Qt::UserRole + 1, "start"}, {Qt::UserRole + 2, "end"}});
        auto *row = new QStandardItem;
        row->setData(.25, Qt::UserRole + 1);
        row->setData(.75, Qt::UserRole + 2);
        marks.appendRow(row); // Rendering model, independent of upstream fixtures.
        session.root->setProperty("markerModel", QVariant::fromValue<QObject *>(&marks));
        session.root->setProperty("waiting", false);
        const auto capture = qEnvironmentVariable("ECHO_PROGRESS_PREVIEW_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1., 1.25}) {
                session.root->setProperty("uiScale", scale);
                QQuickItem *start = nullptr, *end = nullptr;
                QTRY_VERIFY((start = findVisual(slider, "progressClimaxStart-0")));
                QTRY_VERIFY((end = findVisual(slider, "progressClimaxEnd-0")));
                QCOMPARE(start->size(), QSizeF(2, 6));
                QCOMPARE(end->size(), QSizeF(2, 6));
                const auto track = slider->property("background").value<QQuickItem *>();
                QVERIFY(track);
                QVERIFY(qAbs(start->mapToItem(track, {1, 3}).x() - .25 * track->width()) < .01);
                QVERIFY(qAbs(end->mapToItem(track, {1, 3}).x() - .75 * track->width()) < .01);
                const auto image = session.window->grabWindow();
                const auto ratio = image.width() / qreal(session.window->width());
                const auto inside = end->mapToScene({1, .5}) * ratio;
                const auto outside = end->mapToScene({8, .5}) * ratio;
                QVERIFY(image.pixelColor(inside.toPoint()) != image.pixelColor(outside.toPoint()));
                if (!capture.isEmpty())
                    QVERIFY(image.save(capture +
                                       QString("/ticks-%1-%2.png").arg(int(theme)).arg(scale)));
            }
        }
        row->setData(.25, Qt::UserRole + 2);
        QTRY_VERIFY(!findVisual(slider, "progressClimaxEnd-0")->isVisible());
        marks.clear();
        QTRY_VERIFY(!findVisual(slider, "progressClimaxStart-0"));
        session.root->setProperty("markerModel", QVariant::fromValue<QObject *>(nullptr));
        session.theme.setMode(ThemeStore::Light);
        session.root->setProperty("uiScale", 1.);
        session.root->setProperty("waiting", true);
        QTRY_VERIFY(findVisual(slider, "progressBusyOverlay"));
        if (!capture.isEmpty()) {
            // Export actual native frames. The preview is a test-only waiting
            // state, not evidence of a real network underrun.
            QElapsedTimer clock;
            clock.start();
            for (int frame = 0; frame < 120; ++frame) {
                if (frame == 90)
                    session.root->setProperty("waiting", false);
                const auto wait = frame * 33 - clock.elapsed();
                if (wait > 0)
                    QTest::qWait(int(wait));
                QVERIFY(session.window->grabWindow().save(
                    capture + QString("/frame-%1.png").arg(frame, 3, 10, QChar('0'))));
            }
            QVERIFY(!findVisual(slider, "progressBusyOverlay"));
        }
        session.root->setProperty("markerModel", QVariant::fromValue<QObject *>(nullptr));
    }

    void playPauseTransitionKeepsEndpointsAndInterrupts()
    {
        ControlsSession session;
        QVERIFY(session.create());
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            EchoIconButton {
                width: 38; height: 38; padding: 9
                animatePlayPause: true; glyph: "play"; text: "播放"
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *button = qobject_cast<QQuickItem *>(object.data());
        button->setParentItem(session.window->contentItem());
        button->setPosition({610, 170});
        auto *icon = findVisual(button, "playPauseIcon");
        QVERIFY(icon);
        auto *play = findVisual(icon, "playPausePlayIcon");
        auto *pause = findVisual(icon, "playPausePauseIcon");
        QVERIFY(play && pause);
        QTRY_VERIFY(icon->property("ready").toBool());
        QCOMPARE(icon->property("phase").toReal(), 0.);
        const auto capture = qEnvironmentVariable("ECHO_PLAY_PAUSE_ANIMATION_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1., 1.25}) {
                button->setScale(scale);
                QTRY_VERIFY(icon->property("ready").toBool());
                const auto frame = [&] {
                    const auto image = session.window->grabWindow();
                    const auto ratio = image.width() / qreal(session.window->width());
                    const auto rect = button->mapRectToScene(button->boundingRect());
                    return image.copy(
                        QRectF(rect.topLeft() * ratio, rect.size() * ratio).toAlignedRect());
                };
                const auto idle = frame();
                button->setProperty("glyph", "pause");
                QTRY_VERIFY(icon->property("transitioning").toBool());
                QTest::qWait(45);
                const auto phase = icon->property("phase").toReal();
                QVERIFY(phase > 0 && phase < 1);
                const auto middle = frame();
                QVERIFY(middle != idle);
                QTRY_VERIFY(!icon->property("transitioning").toBool());
                QCOMPARE(icon->property("phase").toReal(), 1.);
                QCOMPARE(pause->opacity(), 1.);
                QCOMPARE(pause->scale(), 1.);
                QCOMPARE(play->opacity(), 0.);
                const auto active = frame();
                QVERIFY(active != idle && active != middle);
                if (!capture.isEmpty()) {
                    const auto name = QString("/%1-%2")
                                          .arg(theme == ThemeStore::Dark ? "dark" : "light")
                                          .arg(scale);
                    QVERIFY(idle.save(capture + name + "-play.png"));
                    QVERIFY(middle.save(capture + name + "-transition.png"));
                    QVERIFY(active.save(capture + name + "-pause.png"));
                }
                button->setProperty("glyph", "play");
                QTest::qWait(40);
                const auto reverse = icon->property("phase").toReal();
                button->setProperty("glyph", "pause");
                QVERIFY(qAbs(icon->property("phase").toReal() - reverse) < .05);
                QTRY_COMPARE(icon->property("phase").toReal(), 1.);
                button->setProperty("glyph", "play");
                session.theme.setMotionEnabled(false);
                QCOMPARE(icon->property("phase").toReal(), 0.);
                QVERIFY(!icon->property("transitioning").toBool());
                button->setProperty("glyph", "pause");
                QCOMPARE(icon->property("phase").toReal(), 1.);
                session.theme.setMotionEnabled(true);
                button->setProperty("glyph", "play");
                button->setVisible(false);
                QCOMPARE(icon->property("phase").toReal(), 0.);
                QVERIFY(!icon->property("transitioning").toBool());
                button->setVisible(true);
                QTRY_VERIFY(icon->property("ready").toBool());
                button->setProperty("glyph", "pause");
                session.window->showMinimized();
                QTRY_VERIFY(!icon->property("transitioning").toBool());
                QCOMPARE(icon->property("phase").toReal(), 1.);
                session.window->showNormal();
                QVERIFY(Test::prepareNativeInput(session.window));
                button->setProperty("glyph", "play");
                QTRY_COMPARE(icon->property("phase").toReal(), 0.);
            }
        }
        QScopedPointer<QObject> initiallyPlaying(
            component.createWithInitialProperties({{"glyph", "pause"}}));
        QVERIFY(initiallyPlaying);
        auto *initialIcon =
            findVisual(qobject_cast<QQuickItem *>(initiallyPlaying.data()), "playPauseIcon");
        QVERIFY(initialIcon);
        QCOMPARE(initialIcon->property("phase").toReal(), 1.);
        QVERIFY(!initialIcon->property("transitioning").toBool());
    }

    void progressBusyRendersAndRetiresNativeAnimation()
    {
        ControlsSession session;
        QVERIFY(session.create());
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            EchoSlider {
                width: 500; height: 24; padding: 0; from: 0; to: 100; value: 60
                progress: true; busy: true
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *slider = qobject_cast<QQuickItem *>(object.data());
        QVERIFY(slider);
        slider->setParentItem(session.window->contentItem());
        slider->setX(20);
        slider->setY(240);
        const auto capture = qEnvironmentVariable("ECHO_PROGRESS_BUSY_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto theme : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(theme);
            for (const auto scale : {1., 1.25}) {
                slider->setScale(scale);
                QTRY_VERIFY(findVisual(slider, "progressBusyOverlay"));
                auto *overlay = findVisual(slider, "progressBusyOverlay");
                auto *wave = findVisual(slider, "progressBusyWave");
                QVERIFY(overlay && wave);
                QCOMPARE(overlay->width(), 300.);
                QTRY_VERIFY(overlay->property("animating").toBool());
                const auto frame = [&] {
                    const auto image = session.window->grabWindow();
                    const auto dpr = image.width() / qreal(session.window->width());
                    auto rect = overlay->mapRectToScene(overlay->boundingRect());
                    return image.copy(
                        QRectF(rect.topLeft() * dpr, rect.size() * dpr).toAlignedRect());
                };
                QImage first, second;
                bool changed = false;
                for (int attempt = 0; attempt < 6 && !changed; ++attempt) {
                    first = frame();
                    QTest::qWait(130);
                    second = frame();
                    changed = first != second;
                }
                QVERIFY2(changed, "Native rendered progress pixels must change over time");
                if (!capture.isEmpty()) {
                    const auto name = QString("/%1-%2")
                                          .arg(theme == ThemeStore::Dark ? "dark" : "light")
                                          .arg(scale);
                    QVERIFY(first.save(capture + name + "-sweep-a.png"));
                    QVERIFY(second.save(capture + name + "-sweep-b.png"));
                }
                session.theme.setMotionEnabled(false);
                QTRY_COMPARE(wave->width(), overlay->width());
                QCOMPARE(wave->x(), 0.);
                const auto initialOpacity = wave->opacity();
                QTest::qWait(350);
                QVERIFY(qAbs(wave->opacity() - initialOpacity) > .01);
                slider->setVisible(false);
                QTRY_VERIFY(!overlay->property("animating").toBool());
                const auto stoppedPhase = overlay->property("phase").toReal();
                const auto stoppedPulse = overlay->property("pulse").toReal();
                QTest::qWait(80);
                QCOMPARE(overlay->property("phase").toReal(), stoppedPhase);
                QCOMPARE(overlay->property("pulse").toReal(), stoppedPulse);
                slider->setVisible(true);
                session.window->showMinimized();
                QTRY_VERIFY(!overlay->property("animating").toBool());
                session.window->showNormal();
                QVERIFY(Test::prepareNativeInput(session.window));
                QTRY_VERIFY(overlay->property("animating").toBool());
                session.theme.setMotionEnabled(true);
                slider->setProperty("busy", false);
                QTRY_VERIFY(!findVisual(slider, "progressBusyOverlay"));
                // The range returns to its static accent and frees the animation.
                slider->setProperty("busy", true);
            }
        }
        slider->setProperty("progress", false);
        QTRY_VERIFY(!findVisual(slider, "progressBusyOverlay"));
    }

    void sourcePauseKeepsComparableInkToPlay()
    {
        ControlsSession session;
        QVERIFY(session.create());
        EchoIconImageProvider provider;
        const auto bounds = [](const QImage &image) {
            QRect ink;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (qAlpha(image.pixel(x, y)) >= 128)
                        ink |= QRect(x, y, 1, 1);
            return ink;
        };
        for (const int pixels : {40, 50}) {
            const auto play = provider.requestImage("play/ff000000", nullptr, {pixels, pixels});
            const auto pause = provider.requestImage("pause/ff000000", nullptr, {pixels, pixels});
            const auto oldPause =
                provider.requestImage("player-pause/ff000000", nullptr, {pixels, pixels});
            QVERIFY(!play.isNull() && !pause.isNull() && !oldPause.isNull());
            const auto a = bounds(play), b = bounds(pause), old = bounds(oldPause);
            qInfo() << "Play/pause/queue ink at" << pixels << a << b << old;
            QVERIFY(b.height() >= a.height() * .85);
            QVERIFY(b.width() >= a.width() * .9);
            QVERIFY(b.height() > old.height() && b.width() > old.width());
            QVERIFY(qAbs(b.center().y() - a.center().y()) <= 1);
        }
    }
    void disabledIconHoverIsOptInAndNeverActivates()
    {
        ControlsSession session;
        QVERIFY(session.create());
        auto *button = session.item("iconButton");
        QVERIFY(button);
        auto *background = button->property("background").value<QQuickItem *>();
        auto *icon = button->property("contentItem").value<QQuickItem *>();
        QVERIFY(background && icon);
        QSignalSpy clicks(button, SIGNAL(clicked()));
        const auto capture = qEnvironmentVariable("ECHO_DISABLED_HOVER_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const double zoom : {1., 1.25}) {
                button->setScale(zoom);
                button->setEnabled(false);
                button->setProperty("hoverWhenDisabled", false);
                Test::moveNativePointer(session.window, button->mapToScene({20, 18}).toPoint());
                QTRY_VERIFY(button->property("hovered").toBool());
                QCOMPARE(background->property("color").value<QColor>().alpha(), 0);
                const auto before = session.window->grabWindow();
                button->setProperty("hoverWhenDisabled", true);
                QTRY_VERIFY(button->property("hoverHighlighted").toBool());
                QTRY_VERIFY(background->property("color").value<QColor>().alpha() > 0);
                const auto after = session.window->grabWindow();
                const auto point = button->mapToScene({3, 18}) * after.devicePixelRatio();
                QVERIFY(before.pixelColor(point.toPoint()) != after.pixelColor(point.toPoint()));
                QCOMPARE(icon->opacity(), button->property("disabledOpacity").toReal());
                QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                                  button->mapToScene({20, 18}).toPoint());
                QCOMPARE(clicks.size(), 0);
                if (!capture.isEmpty())
                    QVERIFY(after.save(capture +
                                       QString("/disabled-%1-%2.png").arg(int(mode)).arg(zoom)));
                // A login/modal shield disables an ancestor, not just this action.
                button->parentItem()->setEnabled(false);
                QTRY_VERIFY(!button->property("hoverHighlighted").toBool());
                QCOMPARE(background->property("color").value<QColor>().alpha(), 0);
                button->parentItem()->setEnabled(true);
            }
        }
    }
    void shadowsReleaseTheirNativeMaterials()
    {
        ControlsSession session;
        QVERIFY(session.create());
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QQmlComponent component(&session.engine);
        component.setData(R"(
import QtQuick
import EchoMusic.Controls
Item {
    id: testSurface
    width: 120
    height: 120
    EchoSurfaceShadow {
        objectName: "memoryShadow"
        anchors.fill: parent
        surface: testSurface
        blur: 24
        radius: 10
    }
})",
                          QUrl("memory-shadow.qml"));
        for (const bool attachToWindow : {false, true}) {
            QScopedPointer<QObject> root(component.create());
            QVERIFY2(root, qPrintable(component.errorString()));
            auto *item = qobject_cast<QQuickItem *>(root.data());
            QVERIFY(item);
            if (attachToWindow)
                item->setParentItem(session.window->contentItem());
            auto *shadow = findVisual(item, "memoryShadow");
            QVERIFY(shadow);
            QTest::qWait(150);
            QList<QPointer<QQuickItem>> materials;
            for (auto *child :
                 shadow->findChildren<QQuickItem *>(QString(), Qt::FindDirectChildrenOnly)) {
                if (child->inherits("QQuickShaderEffect")) {
                    materials.append(child);
                    QVERIFY(child->parent() == shadow);
                }
            }
            QVERIFY(materials.size() >= 2); // Native default and the shared custom material.
            root.reset();
            QTRY_VERIFY(std::all_of(materials.cbegin(), materials.cend(),
                                    [](const auto &material) { return material.isNull(); }));
        }
        QVERIFY(warnings.isEmpty());
    }
    void sourceControlRolesKeepOrdinaryActionsAndLargeFormsDistinct()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("ControlSizeHarness.qml")));
        const auto capture = qEnvironmentVariable("ECHO_CONTROL_SIZE_CAPTURE_DIR");
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const auto scale : {1.0, 1.25}) {
                session.root->setProperty("surfaceScale", scale);
                // Shared size roles: Small, Compact, Standard and Large.
                for (int role = 0; role < 4; ++role) {
                    session.root->setProperty("role", role);
                    const int height = QList<int>{28, 32, 36, 56}[role];
                    for (const auto &name : {"sizedAction", "sizedSoftAction", "sizedField",
                                             "sizedSelect", "sizedNumber", "sizedDate"}) {
                        auto *control = session.item(name);
                        QVERIFY(control);
                        QTRY_COMPARE(control->height(), qreal(height));
                        const auto font = control->property("font").value<QFont>();
                        if (QString(name).contains("Action")) {
                            QCOMPARE(font.pixelSize(), role == 3 ? 15 : 12);
                            QCOMPARE(int(font.weight()), role == 3 ? 900 : 600);
                        } else {
                            QCOMPARE(font.pixelSize(), role == 3 ? 15 : role == 2 ? 13 : 12);
                            QCOMPARE(int(font.weight()), 500);
                        }
                    }
                    auto *header = session.item("sizedDrawerHeader");
                    auto *close = findVisual(header, "drawerCloseButton");
                    QVERIFY(close);
                    QCOMPARE(close->size(), QSizeF(32, 32));
                    QTRY_VERIFY(
                        qAbs(close->mapToItem(header, {32, 0}).x() - (header->width() - 24)) < 0.5);
                    if (!capture.isEmpty()) {
                        QVERIFY(QDir().mkpath(capture));
                        session.window->update();
                        QTest::qWait(60);
                        QVERIFY(session.window->grabWindow().save(
                            capture + QString("/roles-%1-%2-%3.png")
                                          .arg(mode == ThemeStore::Light ? "light" : "dark")
                                          .arg(scale)
                                          .arg(role)));
                    }
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void selectProjectionKeepsTypedValuesAndTracksModelChanges()
    {
        EchoSelectModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setSource(QVariantList{
            QVariantMap{{"label", "Alpha"}, {"value", 1}},
            QVariantMap{{"label", "Blocked"}, {"value", "disabled"}, {"disabled", true}},
            QVariantMap{{"label", "Beta"}, {"value", "1"}}});
        QCOMPARE(model.optionForValue(1).value("label").toString(), QString("Alpha"));
        QCOMPARE(model.optionForValue(QString("1")).value("label").toString(), QString("Beta"));
        QCOMPARE(model.nextEnabled(0, 1), 2);
        model.setQuery("bETA");
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.option(0).value("sourceIndex").toInt(), 2);
        QStringListModel live({"One", "Two"});
        model.setQuery({});
        model.setSource(QVariant::fromValue(static_cast<QObject *>(&live)));
        QCOMPARE(model.count(), 2);
        live.setStringList({"Three"});
        QCOMPARE(model.count(), 1);
        QCOMPARE(model.option(0).value("label").toString(), QString("Three"));
    }
    void selectSearchMultiKeyboardPlacementAndVirtualization()
    {
        ControlsSession session;
        session.theme.setMode(ThemeStore::Light);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create("SelectHarness.qml"));
        auto *selector = session.item("featureSelect");
        QVERIFY(selector);
        QPointer<QObject> popup;
        QVERIFY(!selector->property("popup").value<QObject *>());
        auto *projection = selector->property("optionModel").value<EchoSelectModel *>();
        QVERIFY(projection);
        QTRY_COMPARE(projection->count(), 3);
        auto *bound = session.item("indexBoundSelect");
        QVERIFY(bound);
        QSignalSpy activated(bound, SIGNAL(activated(int)));
        bound->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(session.window, Qt::Key_Down);
        QTest::keyClick(session.window, Qt::Key_Return);
        QTRY_COMPARE(bound->property("currentIndex").toInt(), 2);
        QCOMPARE(activated.count(), 1);
        QTRY_VERIFY(!bound->property("popup").value<QObject *>());
        // Native presentation setters must preserve an application's binding.
        session.root->setProperty("indexSelection", 1);
        QTRY_COMPARE(bound->property("currentIndex").toInt(), 1);
        session.root->setProperty("indexSelection", 0);
        QTRY_COMPARE(bound->property("currentIndex").toInt(), 0);
        session.root->setProperty("selected", 1);
        QTRY_COMPARE(selector->property("currentIndex").toInt(), 0);
        auto *input = findVisual(selector, "selectSearchInput");
        QVERIFY(input);
        input->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(input->hasActiveFocus());
        for (const auto key : QByteArray("Beta"))
            QTest::keyClick(session.window, key);
        QTRY_VERIFY((popup = selector->property("popup").value<QObject *>()));
        QTRY_VERIFY(popup->property("opened").toBool());
        QTRY_COMPARE(selector->property("searchTerm").toString(), QString("Beta"));
        QTRY_COMPARE(projection->count(), 1);
        const auto searchPoint =
            input->mapToScene(QPointF(input->width() / 2, input->height() / 2)).toPoint();
        Test::moveNativePointer(session.window, searchPoint);
        QTRY_VERIFY(selector->property("hovered").toBool());
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, searchPoint);
        QTRY_VERIFY(input->hasActiveFocus());
        // Clicking the search trigger must not consume the underlying input.
        QTRY_VERIFY((popup = selector->property("popup").value<QObject *>()));
        QTRY_VERIFY(popup->property("opened").toBool());
        QCOMPARE(selector->property("searchTerm").toString(), QString("Beta"));
        const auto captures = qEnvironmentVariable("ECHO_SELECT_COLOR_CAPTURES");
        const auto capture = [&](const QString &name) {
            if (captures.isEmpty())
                return true;
            QDir().mkpath(captures);
            return session.window->grabWindow().save(captures + "/" + name + ".png");
        };
        QVERIFY(capture("select-search"));
        const auto selectImage = session.window->grabWindow();
        if (!selectImage.isNull() &&
            session.window->rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
            auto *surface = popup->property("background").value<QQuickItem *>();
            QVERIFY(surface);
            const auto below =
                surface->mapToScene(QPointF(surface->width() / 2, surface->height() + 3));
            const auto ratio = selectImage.devicePixelRatio();
            const auto shadow =
                selectImage.pixelColor(qRound(below.x() * ratio), qRound(below.y() * ratio));
            QVERIFY2(shadow.red() < 250,
                     "Popover shadow must extend beyond its rounded surface body");
        }
        QTest::keyClick(session.window, Qt::Key_Down);
        QTest::keyClick(session.window, Qt::Key_Return);
        QTRY_COMPARE(selector->property("currentIndex").toInt(), 2);
        QCOMPARE(session.root->property("selected").toString(), QString("1"));
        QTRY_VERIFY(popup.isNull());
        session.root->setProperty("multi", true);
        session.root->setProperty("selected", QVariantList{1});
        QVERIFY(QMetaObject::invokeMethod(selector, "openPopup"));
        QTRY_VERIFY((popup = selector->property("popup").value<QObject *>()));
        QTRY_VERIFY(popup->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(selector, "choose", Q_ARG(int, 1)));
        QCOMPARE(session.root->property("selected").toList().size(), 1);
        QVERIFY(QMetaObject::invokeMethod(selector, "choose", Q_ARG(int, 2)));
        QTRY_COMPARE(session.root->property("selected").toList().size(), 2);
        QCOMPARE(selector->property("overflowCount").toInt(), 1);
        QVERIFY(capture("select-multi"));
        QVERIFY(popup->property("visible").toBool());
        auto *removeTag = findVisual(selector, "selectTagRemoveAction");
        QVERIFY(removeTag);
        const auto removePoint =
            removeTag->mapToScene(QPointF(removeTag->width() / 2, removeTag->height() / 2))
                .toPoint();
        Test::moveNativePointer(session.window, removePoint);
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, removePoint);
        QTRY_COMPARE(session.root->property("selected").toList().size(), 1);
        QCOMPARE(session.root->property("selected").toList().first().toString(), QString("1"));
        QCOMPARE(selector->property("overflowCount").toInt(), 0);
        QVERIFY(popup->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(selector, "choose", Q_ARG(int, 0)));
        auto *clearAction = findVisual(selector, "selectIndicatorAction");
        QVERIFY(clearAction);
        const auto clearPoint =
            clearAction->mapToScene(QPointF(clearAction->width() / 2, clearAction->height() / 2))
                .toPoint();
        Test::moveNativePointer(session.window, clearPoint);
        QTRY_VERIFY(selector->property("showClear").toBool());
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, clearPoint);
        QTRY_VERIFY(session.root->property("selected").toList().isEmpty());
        QVERIFY(popup->property("visible").toBool());
        QMetaObject::invokeMethod(popup, "close");
        QTRY_VERIFY(popup.isNull());
        session.root->setProperty("multi", false);
        session.root->setProperty("selected", QString());
        QTRY_VERIFY(!selector->property("showClear").toBool());
        // Unknown nonempty values still expose clear; an empty value does not.
        session.root->setProperty("selected", 0);
        // Popup teardown resets native hover. Re-enter before checking a
        // hover-only affordance, rather than relying on a stale cursor state.
        Test::moveNativePointer(session.window, QPoint(740, 20));
        QTRY_VERIFY(!selector->property("hovered").toBool());
        Test::moveNativePointer(session.window, clearPoint);
        QTRY_VERIFY(selector->property("hovered").toBool());
        QTRY_VERIFY(selector->property("showClear").toBool());
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, clearPoint);
        QTRY_COMPARE(session.root->property("selected").toString(), QString());
        session.root->setProperty("multi", true);
        QVariantList large;
        for (int i = 0; i < 50000; ++i)
            large.append(QVariantMap{{"label", QString("Option %1 中文").arg(i)}, {"value", i}});
        session.root->setProperty("items", large);
        session.root->setProperty("zoom", 1.25);
        selector->setPosition({540, 450});
        QVERIFY(QMetaObject::invokeMethod(selector, "openPopup"));
        QTRY_VERIFY((popup = selector->property("popup").value<QObject *>()));
        QTRY_VERIFY(popup->property("opened").toBool());
        QTRY_COMPARE(projection->count(), 50000);
        const auto scale = popup->property("uiScale").toDouble();
        const auto x = popup->property("x").toDouble(), y = popup->property("y").toDouble();
        QVERIFY(x >= 12 && y >= 12);
        QVERIFY(x + popup->property("width").toDouble() * scale <= session.window->width() - 12);
        QVERIFY(y + popup->property("height").toDouble() * scale <= session.window->height() - 12);
        int delegates = 0;
        const auto count = [&](auto &&self, QQuickItem *item) -> void {
            if (item->objectName().startsWith("selectOption-"))
                ++delegates;
            for (auto *child : item->childItems())
                self(self, child);
        };
        count(count, session.window->contentItem());
        QVERIFY(delegates > 0 && delegates < 30);
        auto *list = popup->property("contentItem").value<QQuickItem *>();
        QVERIFY(list);
        list->setProperty("contentY", 1000000.);
        QTest::qWait(60);
        delegates = 0;
        count(count, session.window->contentItem());
        QVERIFY(delegates < 40);
        selector->setProperty("searchTerm", "no matching option");
        QTRY_COMPARE(projection->count(), 0);
        QMetaObject::invokeMethod(popup, "close");
        QTRY_VERIFY(popup.isNull());
        session.root->setProperty(
            "items",
            QVariantList{QVariantMap{{"label", QString(160, QChar(u'长'))}, {"value", 9}}});
        session.root->setProperty("wrapped", true);
        QVERIFY(QMetaObject::invokeMethod(selector, "openPopup"));
        QTRY_VERIFY((popup = selector->property("popup").value<QObject *>()));
        QTRY_VERIFY(popup->property("opened").toBool());
        QQuickItem *wrapped = nullptr;
        QTRY_VERIFY((wrapped = session.item("selectOption-0")));
        QTRY_VERIFY(wrapped->height() > 36);
        QVERIFY(capture("select-wrap"));
        selector->setEnabled(false);
        QTRY_VERIFY(popup.isNull());
        QCOMPARE(warnings.count(), 0);
    }
    void backdropSourceIsWindowLocalAndReleasedWhenHidden()
    {
        QQuickWindow first, second;
        auto firstRoot = std::make_unique<EchoBackdropRoot>(first.contentItem());
        EchoBackdropRoot secondRoot(second.contentItem());
        QQuickItem popupParent(first.contentItem());
        QQuickItem surface(&popupParent);
        surface.setSize({200, 100});
        surface.setPosition({10, 20});
        popupParent.setPosition({80, 100});
        popupParent.setScale(1.25);
        EchoBackdropSampler sampler;
        sampler.setSurface(&surface);
        QVERIFY(!sampler.source());
        sampler.setEnabled(true);
        QCOMPARE(sampler.source(), firstRoot.get());
        QCOMPARE(sampler.sourceRect(), QRectF(62.5, 95, 310, 185));
        QSignalSpy geometry(&sampler, &EchoBackdropSampler::changed);
        popupParent.setX(100);
        QVERIFY(!geometry.isEmpty());
        QCOMPARE(sampler.sourceRect().x(), 82.5);
        surface.setParentItem(second.contentItem());
        QCOMPARE(sampler.source(), &secondRoot);
        surface.setParentItem(&popupParent);
        auto pageRoot = std::make_unique<EchoBackdropRoot>(first.contentItem());
        QCOMPARE(sampler.source(), pageRoot.get());
        pageRoot->setSamplingActive(false);
        QCOMPARE(sampler.source(), firstRoot.get());
        pageRoot->setSamplingActive(true);
        QCOMPARE(sampler.source(), pageRoot.get());
        firstRoot.reset();
        QCOMPARE(sampler.source(), pageRoot.get());
        pageRoot.reset();
        QVERIFY(!sampler.source());
        sampler.setEnabled(false);
        QVERIFY(sampler.sourceRect().isEmpty());
    }
    void floatingFrostSamplesTheSceneAndStopsOnHide()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("BackdropHarness.qml")));
        auto *surface = session.item("frostProbe");
        QVERIFY(surface);
        QVERIFY(!surface->property("hasBackdrop").toBool());
        qInfo() << "Backdrop graphics API:" << session.window->rendererInterface()->graphicsApi();
        session.theme.setFloatingFrosted(true);
        QTRY_VERIFY(surface->property("hasBackdrop").toBool());
        if (QGuiApplication::platformName() != "offscreen" &&
            session.window->rendererInterface()->graphicsApi() != QSGRendererInterface::Software) {
            QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
            session.window->update();
            QTRY_VERIFY(!frames.isEmpty());
            QTest::qWait(100);
            const auto image = session.window->grabWindow();
            QVERIFY(!image.isNull());
            const auto dpr = session.window->devicePixelRatio();
            const auto pixel = [&](int x) {
                return image.pixelColor(qRound(x * dpr), qRound(160 * dpr));
            };
            // The 18% exposed backdrop must differ across the red/blue scene;
            // around the boundary it must contain both colors, not a flat tint.
            const auto left = pixel(175), middle = pixel(220), right = pixel(265);
            QVERIFY(left.red() > right.red() + 20);
            QVERIFY(right.blue() > left.blue() + 20);
            QVERIFY(middle.red() > right.red() + 5);
            QVERIFY(middle.blue() > left.blue() + 5);
            // CSS sigma=24 spreads the edge well beyond a 24px radius. A
            // translucent tint or the former narrow MultiEffect blur fails here.
            QVERIFY(pixel(196).blue() > left.blue() + 3);
            QVERIFY(pixel(244).red() > right.red() + 3);
            qInfo() << "Frost edge pixels:" << left << pixel(196) << middle << pixel(244) << right;
            const auto captures = qEnvironmentVariable("ECHO_THEME_SCREENSHOTS");
            if (!captures.isEmpty()) {
                QDir().mkpath(captures);
                QVERIFY(image.save(captures + "/floating-frost-edge.png"));
            }
        }
        surface->setVisible(false);
        QTRY_VERIFY(!surface->property("hasBackdrop").toBool());
        surface->setVisible(true);
        QTRY_VERIFY(surface->property("hasBackdrop").toBool());
        session.theme.setFloatingFrosted(false);
        QTRY_VERIFY(!surface->property("hasBackdrop").toBool());
        QCOMPARE(warnings.count(), 0);
    }
    void floatingEffectsAreOwnedByVisibleSurfaces()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("BackdropHarness.qml")));
        session.window->resize(1150, 750);
        auto *surface = session.item("frostProbe");
        QVERIFY(surface);
        surface->setSize({600, 620});
        surface->setVisible(false);
        session.theme.setFloatingFrosted(true);
        const int holdMs =
            qBound(0, qEnvironmentVariableIntValue("ECHO_FLOATING_PROFILE_MS"), 30000);
        QTimer sceneMotion;
        if (qEnvironmentVariableIsSet("ECHO_FLOATING_ANIMATED")) {
            auto *backdropRoot = session.root->findChild<EchoBackdropRoot *>();
            QVERIFY(backdropRoot && !backdropRoot->childItems().isEmpty());
            auto *movingItem = backdropRoot->childItems().first();
            connect(&sceneMotion, &QTimer::timeout, movingItem, [movingItem, frame = 0]() mutable {
                const auto position = frame++ % 120;
                movingItem->setX(position < 60 ? position / 3. : (120 - position) / 3.);
            });
            sceneMotion.start(16);
        }
        const auto samplerCount = [&] {
            QSet<QObject *> visited;
            int count = 0;
            const auto visit = [&](auto &&self, QObject *object) -> void {
                if (!object || visited.contains(object))
                    return;
                visited.insert(object);
                count += qobject_cast<EchoBackdropSampler *>(object) != nullptr;
                for (auto *child : object->children())
                    self(self, child);
                if (auto *item = qobject_cast<QQuickItem *>(object))
                    for (auto *child : item->childItems())
                        self(self, child);
            };
            visit(visit, session.root.data());
            return count;
        };
        const auto phase = [&](const char *name) {
            QTest::qWait(150);
            qInfo() << "Floating phase" << name << "pid" << QCoreApplication::applicationPid()
                    << "samplers" << samplerCount();
            const auto cpu = std::clock();
            QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
            QElapsedTimer wall;
            wall.start();
            QTest::qWait(qMax(100, holdMs));
            qInfo() << "Floating CPU" << name << "wall-ms" << wall.elapsed() << "cpu-ms"
                    << 1000. * (std::clock() - cpu) / CLOCKS_PER_SEC << "frames" << frames.size();
        };
        phase("empty");
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            Item {
                property int amount: 32
                Repeater {
                    model: parent.amount
                    EchoFloatingSurface { width: 200; height: 180; visible: false }
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> bulk(component.create());
        QVERIFY2(bulk, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(bulk.data());
        QVERIFY(host);
        host->setParent(session.root.data());
        host->setParentItem(session.window->contentItem());
        if (holdMs)
            host->setProperty("amount", 500);
        phase("hidden");
        const auto hiddenSamplers = samplerCount();
        surface->setProperty("frostedEnabled", false);
        surface->setVisible(true);
        phase("plain");
        const auto plainSamplers = samplerCount();
        surface->setProperty("frostedEnabled", true);
        QTRY_VERIFY(surface->property("hasBackdrop").toBool());
        phase("frost");
        const auto active = surface->findChildren<EchoBackdropSampler *>();
        QVERIFY(!active.isEmpty());
        QPointer<EchoBackdropSampler> ownedSampler = active.first();
        QList<QPointer<EchoBackdropSampler>> retiredSamplers;
        for (int i = 0; i < 20; ++i) {
            for (auto *sampler : surface->findChildren<EchoBackdropSampler *>())
                retiredSamplers.append(sampler);
            surface->setVisible(false);
            QTest::qWait(20);
            surface->setVisible(true);
            QTRY_VERIFY(surface->property("hasBackdrop").toBool());
            QTest::qWait(20);
        }
        surface->setVisible(false);
        phase("closed-after-20");
        bulk.reset();
        session.engine.collectGarbage();
        phase("destroyed");
        QCOMPARE(hiddenSamplers, 0);
        QCOMPARE(plainSamplers, 0);
        QVERIFY(ownedSampler.isNull());
        QCOMPARE(samplerCount(), 0);
        for (const auto &sampler : retiredSamplers)
            QVERIFY(sampler.isNull());
        surface->setVisible(true);
        QTRY_VERIFY(surface->property("hasBackdrop").toBool());
        session.window->hide();
        QTRY_COMPARE(samplerCount(), 0);
        session.window->show();
        QTRY_VERIFY(surface->property("hasBackdrop").toBool());
        session.window->showMinimized();
        QTRY_COMPARE(samplerCount(), 0);
        session.window->showNormal();
        QTRY_VERIFY(surface->property("hasBackdrop").toBool());
        // The window contentItem's visibility is owned by QQuickWindow's
        // asynchronous expose/minimize path. Exercise an ordinary ancestor.
        QQuickItem ancestor(session.window->contentItem());
        surface->setParentItem(&ancestor);
        ancestor.setVisible(false);
        QTRY_COMPARE(samplerCount(), 0);
        ancestor.setVisible(true);
        QTRY_VERIFY(surface->property("hasBackdrop").toBool());
        surface->setParentItem(session.window->contentItem());
        surface->setProperty("frostedEnabled", false);
        QTRY_COMPARE(samplerCount(), 0);
        QCOMPARE(warnings.size(), 0);
    }
    void menusDrawersAndFileDialogsReleaseAfterClose()
    {
        ControlsSession session;
        QVERIFY(session.create());
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import QtQuick.Controls.Basic
            import QtQuick.Dialogs as NativeDialogs
            import EchoMusic.Controls
            Item {
                id: host
                width: 300; height: 220
                property bool systemFileDialog: false
                property alias menuOwner: menuOwner
                property alias drawerOwner: drawerOwner
                property alias fileOwner: fileOwner
                function openMenu(): void { menuOwner.popup(host, 0, 60); }
                EchoPopupLoader {
                    id: menuOwner
                    hostItem: host
                    sourceComponent: EchoMenu {
                        popupType: Popup.Window
                        Action { text: "菜单动作" }
                    }
                }
                EchoPopupLoader {
                    id: drawerOwner
                    hostItem: host
                    sourceComponent: EchoDrawer {
                        preferredWidth: 260
                        contentItem: Text { text: "抽屉正文" }
                    }
                }
                EchoFileDialogLoader {
                    id: fileOwner
                    hostItem: host
                    sourceComponent: NativeDialogs.FileDialog {
                        options: host.systemFileDialog ? 0 : NativeDialogs.FileDialog.DontUseNativeDialog
                    }
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(root.data());
        QVERIFY(host);
        host->setParent(session.root.data());
        host->setParentItem(session.window->contentItem());
        for (const auto *name : {"menuOwner", "drawerOwner", "fileOwner"}) {
            auto *owner = root->property(name).value<QObject *>();
            QVERIFY(owner);
            QVERIFY(!owner->property("item").value<QObject *>());
            for (int cycle = 0; cycle < 3; ++cycle) {
                if (QByteArray(name) == "menuOwner")
                    QVERIFY(QMetaObject::invokeMethod(root.data(), "openMenu"));
                else if (QByteArray(name) == "fileOwner")
                    QVERIFY(QMetaObject::invokeMethod(owner, "open"));
                else
                    QVERIFY(
                        QMetaObject::invokeMethod(owner, "open", Q_ARG(QVariant, QVariantMap{})));
                QPointer<QObject> popup = owner->property("item").value<QObject *>();
                QVERIFY(popup);
                QTRY_VERIFY(popup->property("visible").toBool());
                QVERIFY(QMetaObject::invokeMethod(popup, "close"));
                QTRY_VERIFY(popup.isNull());
                QVERIFY(!owner->property("item").value<QObject *>());
            }
        }
        if (QGuiApplication::platformName() == "cocoa") {
            root->setProperty("systemFileDialog", true);
            auto *owner = root->property("fileOwner").value<QObject *>();
            QVERIFY(QMetaObject::invokeMethod(owner, "open"));
            QPointer<QObject> native = owner->property("item").value<QObject *>();
            QVERIFY(native);
            QTRY_VERIFY(native->property("visible").toBool());
            QCOMPARE(native->property("parentWindow").value<QWindow *>(), session.window);
            QVERIFY(QMetaObject::invokeMethod(native, "close"));
            QTRY_VERIFY(native.isNull());
            QVERIFY(!owner->property("item").value<QObject *>());
        }
        QCOMPARE(warnings.size(), 0);
    }
    void businessPopupsAreCreatedOnDemandAndDestroyedAfterExit()
    {
        ControlsSession session;
        QVERIFY(session.create());
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            Item {
                id: host
                property alias owner: owner
                property alias picker: picker
                property alias calendar: calendar
                EchoSelect { id: picker; width: 180; options: [{label:"One",value:1},{label:"Two",value:2}] }
                EchoDatePicker { id: calendar; y: 45; width: 180 }
                EchoPopupLoader {
                    id: owner
                    hostItem: host
                    sourceComponent: EchoDialog {
                        heading: "按需窗口"
                        body: EchoSelect { objectName: "nestedSelect"; options: [{label:"nested",value:1}] }
                        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: 180 } }
                    }
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(root.data());
        QVERIFY(host);
        host->setParentItem(session.window->contentItem());
        host->setParent(session.root.data());
        auto *owner = root->property("owner").value<QObject *>();
        auto *picker = root->property("picker").value<QObject *>();
        QVERIFY(owner && picker);
        const auto nativePopups = [&] {
            QSet<QObject *> visited;
            QList<QObject *> found;
            const auto visit = [&](auto &&self, QObject *object) -> void {
                if (!object || visited.contains(object))
                    return;
                visited.insert(object);
                if (object->inherits("QQuickPopup"))
                    found.append(object);
                for (auto *child : object->children())
                    self(self, child);
                if (auto *item = qobject_cast<QQuickItem *>(object))
                    for (auto *child : item->childItems())
                        self(self, child);
            };
            visit(visit, host);
            return found;
        };
        QCOMPARE(nativePopups().size(), 0);
        QVERIFY(!owner->property("item").value<QObject *>());
        for (int cycle = 0; cycle < 10; ++cycle) {
            QVERIFY(QMetaObject::invokeMethod(owner, "open", Q_ARG(QVariant, QVariantMap{})));
            QTRY_VERIFY(owner->property("opened").toBool());
            QPointer<QObject> popup = owner->property("item").value<QObject *>();
            QVERIFY(popup);
            QCOMPARE(nativePopups().size(), 1);
            auto *content = popup->property("contentItem").value<QQuickItem *>();
            QVERIFY(content);
            auto *nested = findVisual(content, "nestedSelect");
            QVERIFY(nested);
            QVERIFY(QMetaObject::invokeMethod(nested, "openPopup"));
            QTRY_COMPARE(nativePopups().size(), 2);
            QPointer<QObject> nestedPopup = nested->property("popup").value<QObject *>();
            QVERIFY(nestedPopup);
            QVERIFY(QMetaObject::invokeMethod(owner, "close"));
            QVERIFY(popup); // Native exit transition must retain the surface.
            if (cycle == 0) {
                // Reopen before exit ends: the queued disposal must not win.
                QVERIFY(QMetaObject::invokeMethod(owner, "open", Q_ARG(QVariant, QVariantMap{})));
                QTRY_VERIFY(owner->property("opened").toBool());
                QCOMPARE(owner->property("item").value<QObject *>(), popup.data());
                QVERIFY(QMetaObject::invokeMethod(owner, "close"));
            }
            QTRY_VERIFY(popup.isNull());
            QTRY_VERIFY(nestedPopup.isNull());
            QTRY_COMPARE(nativePopups().size(), 0);
            QVERIFY(!owner->property("item").value<QObject *>());
        }
        QVERIFY(QMetaObject::invokeMethod(picker, "openPopup"));
        QTRY_COMPARE(nativePopups().size(), 1);
        QPointer<QObject> popup = picker->property("popup").value<QObject *>();
        QVERIFY(popup);
        host->setVisible(false);
        QTRY_VERIFY(popup.isNull());
        QCOMPARE(nativePopups().size(), 0);
        host->setVisible(true);
        QVERIFY(QMetaObject::invokeMethod(owner, "open", Q_ARG(QVariant, QVariantMap{})));
        QTRY_VERIFY(owner->property("opened").toBool());
        popup = owner->property("item").value<QObject *>();
        session.window->hide();
        QTRY_VERIFY(popup.isNull());
        session.window->show();
        QVERIFY(Test::prepareNativeInput(session.window));
        QVERIFY(QMetaObject::invokeMethod(owner, "open", Q_ARG(QVariant, QVariantMap{})));
        QTRY_VERIFY(owner->property("opened").toBool());
        popup = owner->property("item").value<QObject *>();
        root.reset();
        QTRY_VERIFY(popup.isNull());
        QCOMPARE(warnings.size(), 0);
    }
    void tooltipPopupsExistOnlyDuringTheirTrigger()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("BackdropHarness.qml")));
        session.item("frostProbe")->setVisible(false);
        const auto nativeTips = [&] {
            QSet<QObject *> visited;
            QList<QObject *> found;
            const auto visit = [&](auto &&self, QObject *object) -> void {
                if (!object || visited.contains(object))
                    return;
                visited.insert(object);
                if (object->inherits("QQuickToolTip"))
                    found.append(object);
                for (auto *child : object->children())
                    self(self, child);
                if (auto *item = qobject_cast<QQuickItem *>(object))
                    for (auto *child : item->childItems())
                        self(self, child);
            };
            visit(visit, session.root.data());
            return found;
        };
        const auto phase = [&](const char *name) {
            QTest::qWait(100);
            qInfo() << "Tooltip phase" << name << "pid" << QCoreApplication::applicationPid()
                    << "native-tooltips" << nativeTips().size();
            QTest::qWait(
                qBound(20, qEnvironmentVariableIntValue("ECHO_TOOLTIP_PROFILE_MS"), 30000));
        };
        phase("empty");
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            Item {
                Repeater {
                    model: 500
                    Item {
                        id: anchor
                        width: 32
                        height: 32
                        y: 100
                        property alias hint: hint
                        EchoToolTip {
                            id: hint
                            parent: anchor
                            text: "有必要的说明"
                            delay: 0
                        }
                    }
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> bulk(component.create());
        QVERIFY2(bulk, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(bulk.data());
        QVERIFY(host);
        host->setParent(session.root.data());
        host->setParentItem(session.window->contentItem());
        phase("configured-500");
        const auto idleCount = nativeTips().size();
        QObject *hint = nullptr;
        for (auto *anchor : host->childItems()) {
            hint = anchor->property("hint").value<QObject *>();
            if (hint)
                break;
        }
        QVERIFY(hint);
        hint->setProperty("requestedVisible", true);
        QTRY_VERIFY(hint->property("visible").toBool());
        phase("triggered-one");
        const auto triggeredCount = nativeTips().size();
        QList<QPointer<QObject>> retired;
        for (auto *tip : nativeTips())
            if (tip->property("visible").toBool())
                retired.append(tip);
        hint->setProperty("requestedVisible", false);
        phase("left");
        const auto leftCount = nativeTips().size();
        for (int i = 0; i < 20; ++i) {
            hint->setProperty("requestedVisible", true);
            QTRY_VERIFY(hint->property("visible").toBool());
            for (auto *tip : nativeTips())
                if (tip->property("visible").toBool())
                    retired.append(tip);
            hint->setProperty("requestedVisible", false);
            QTest::qWait(10);
        }
        phase("left-after-20");
        const auto finalCount = nativeTips().size();
        hint->setProperty("delay", 500);
        for (int i = 0; i < 5; ++i) {
            hint->setProperty("requestedVisible", true);
            QTest::qWait(20);
            QVERIFY(nativeTips().isEmpty());
            hint->setProperty("requestedVisible", false);
        }
        QTest::qWait(550);
        QVERIFY(nativeTips().isEmpty());
        bulk.reset();
        session.engine.collectGarbage();
        phase("destroyed");
        QCOMPARE(idleCount, 0);
        QCOMPARE(triggeredCount, 1);
        QCOMPARE(leftCount, 0);
        QCOMPARE(finalCount, 0);
        for (const auto &tip : retired)
            QVERIFY(tip.isNull());
        QCOMPARE(warnings.size(), 0);
    }
    void nativeMenuCancelsVisibleAndDelayedHintsUntilNewHover()
    {
        ControlsSession session;
        QVERIFY(session.create());
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QQmlComponent component(&session.engine);
        component.setData(R"(
            import QtQuick
            import EchoMusic.Controls
            Item {
                id: anchor
                width: 32
                height: 32
                property alias hint: hint
                function nativeMenu() { EchoPopupCoordinator.nativeMenuOpening(); }
                EchoToolTip {
                    id: hint
                    parent: anchor
                    text: "说明操作范围"
                    delay: 150
                }
            }
        )",
                          QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        auto *anchor = qobject_cast<QQuickItem *>(object.data());
        anchor->setParentItem(session.window->contentItem());
        auto *hint = object->property("hint").value<QObject *>();
        QVERIFY(hint);
        hint->setProperty("requestedVisible", true);
        QVERIFY(!hint->property("popup").value<QObject *>());
        QVERIFY(QMetaObject::invokeMethod(object.data(), "nativeMenu"));
        QTest::qWait(180);
        QVERIFY(!hint->property("popup").value<QObject *>());
        hint->setProperty("requestedVisible", false);
        hint->setProperty("delay", 0);
        hint->setProperty("requestedVisible", true);
        QTRY_VERIFY(hint->property("visible").toBool());
        QPointer<QObject> popup = hint->property("popup").value<QObject *>();
        QVERIFY(popup);
        QVERIFY(QMetaObject::invokeMethod(object.data(), "nativeMenu"));
        QTRY_VERIFY(popup.isNull());
        QVERIFY(!hint->property("visible").toBool());
        QTest::qWait(180);
        QVERIFY(!hint->property("popup").value<QObject *>());
        hint->setProperty("requestedVisible", false);
        hint->setProperty("requestedVisible", true);
        QTRY_VERIFY(hint->property("visible").toBool());
        QCOMPARE(warnings.size(), 0);
    }
    void numberInputCommitsAndClampsAcrossAllInputPaths()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        auto *number = session.item("numberInput");
        QVERIFY(number);
        const auto point = [&](qreal x, qreal y) {
            return number->mapToScene(QPointF(x, y)).toPoint();
        };
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          point(number->width() - 14, number->height() / 4));
        QTRY_COMPARE(number->property("value").toInt(), 8);
        QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier,
                          point(number->width() - 14, number->height() / 4));
        QTest::qWait(550);
        QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier,
                            point(number->width() - 14, number->height() / 4));
        QTRY_COMPARE(number->property("value").toInt(), 9);
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          point(number->width() - 14, number->height() * .75));
        QTRY_COMPARE(number->property("value").toInt(), 8);
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, point(30, 20));
        QTest::keyClick(session.window, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(session.window, Qt::Key_Backspace);
        QTest::keyClick(session.window, Qt::Key_3);
        QTest::keyClick(session.window, Qt::Key_Return);
        QTRY_COMPARE(number->property("value").toInt(), 3);
        number->setEnabled(false);
        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                          point(number->width() - 14, number->height() / 4));
        QCOMPARE(number->property("value").toInt(), 3);
        QCOMPARE(warnings.size(), 0);
    }
    void dialogHeaderBodyAndFooterShareInsets()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("DialogLayoutHarness.qml")));
        auto *panel = session.root->property("panel").value<QObject *>();
        QVERIFY(panel);
        const auto capture = qEnvironmentVariable("ECHO_DIALOG_LAYOUT_CAPTURE_DIR");
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const auto scale : {1.0, 1.25}) {
                session.root->setProperty("surfaceScale", scale);
                for (const bool withActions : {false, true}) {
                    session.root->setProperty("withActions", withActions);
                    session.root->setProperty(
                        "heading",
                        withActions
                            ? QStringLiteral("很长的弹窗标题 Long dialog heading 与数量标签")
                            : QStringLiteral("切换账号"));
                    for (const bool flush : {false, true}) {
                        session.root->setProperty("flushBody", flush);
                        session.root->setProperty("topInset", flush ? 26 : 24);
                        QVERIFY(QMetaObject::invokeMethod(panel, "open"));
                        QTRY_VERIFY(panel->property("opened").toBool());
                        auto *close = session.item("dialogCloseButton");
                        auto *title = session.item("dialogHeadingTitle");
                        auto *actions = session.item("dialogHeaderActions");
                        auto *body = session.item("dialogBodyContentProbe");
                        auto *footer = session.item("dialogFooter");
                        QVERIFY(close && title && actions && body && footer);
                        const auto bounds = [](QQuickItem *item) {
                            return item->mapRectToScene(
                                QRectF(0, 0, item->width(), item->height()));
                        };
                        const auto right = panel->property("x").toReal() +
                                           (panel->property("width").toReal() - 24) * scale;
                        QTRY_VERIFY(qAbs(bounds(close).right() - right) < 0.5);
                        QCOMPARE(close->size(), QSizeF(32, 32));
                        QVERIFY(qAbs(bounds(close).top() -
                                     (panel->property("y").toReal() +
                                      panel->property("topPadding").toReal() * scale)) < 0.5);
                        QTRY_VERIFY(qAbs(bounds(body).right() - right) < 0.5);
                        QTRY_VERIFY(qAbs(bounds(footer).right() - right) < 0.5);
                        QTRY_VERIFY(qAbs(bounds(title).left() - bounds(body).left()) < 0.5);
                        if (withActions) {
                            QVERIFY(qAbs(bounds(actions).top() - bounds(close).top()) < 0.5);
                            QTRY_VERIFY(bounds(title).right() + 12 * scale <=
                                        bounds(actions).left() + 0.5);
                            QVERIFY(bounds(actions).right() + 8 * scale <=
                                    bounds(close).left() + 0.5);
                        } else {
                            QVERIFY(bounds(title).right() + 8 * scale <=
                                    bounds(close).left() + 0.5);
                        }
                        if (!capture.isEmpty() && !flush) {
                            QVERIFY(QDir().mkpath(capture));
                            session.window->update();
                            QTest::qWait(60);
                            QVERIFY(session.window->grabWindow().save(
                                capture + QString("/dialog-%1-%2-%3.png")
                                              .arg(mode == ThemeStore::Light ? "light" : "dark")
                                              .arg(scale)
                                              .arg(withActions)));
                        }
                        QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier,
                                          bounds(close).center().toPoint());
                        QTRY_VERIFY(!panel->property("visible").toBool());
                    }
                }
            }
        }
        session.root->setProperty("closable", false);
        QVERIFY(QMetaObject::invokeMethod(panel, "open"));
        QTRY_VERIFY(panel->property("opened").toBool());
        QVERIFY(!session.item("dialogCloseButton")->isVisible());
        QVERIFY(QMetaObject::invokeMethod(panel, "close"));
        QCOMPARE(warnings.size(), 0);
    }
    void drawerFixedActionsSurviveLongTitlesAndVisibilityChanges()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("DialogLayoutHarness.qml")));
        session.window->resize(980, 900);
        session.root->setProperty("showDrawer", true);
        auto *header = session.item("allocationDrawerHeader");
        auto *actions = session.item("drawerHeaderActions");
        auto *title = session.item("drawerHeadingTitle");
        auto *close = session.item("drawerCloseButton");
        QVERIFY(header && actions && title && close);
        session.theme.setMotionEnabled(false);
        const auto capture = qEnvironmentVariable("ECHO_LAYOUT_ALLOCATION_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const auto scale : {1.0, 1.25}) {
                session.root->setProperty("surfaceScale", scale);
                for (const int width : {360, 440, 720}) {
                    session.root->setProperty("drawerWidth", width);
                    session.root->setProperty(
                        "heading",
                        QStringLiteral("很长的抽屉标题 Long header 中文与 Latin 字体回退"));
                    for (const bool withActions : {false, true}) {
                        session.root->setProperty("withActions", withActions);
                        for (const bool loading : {false, true}) {
                            session.root->setProperty("drawerLoading", loading);
                            session.root->setProperty("secondaryAction", loading);
                            QTRY_COMPARE(actions->width(), actions->implicitWidth());
                            QTRY_VERIFY(qAbs(close->mapToItem(header, {32, 0}).x() - (width - 24)) <
                                        .5);
                            QTRY_VERIFY(title->mapToItem(header, {title->width(), 0}).x() + 12 <=
                                        actions->mapToItem(header, QPointF()).x() + .5);
                            QTRY_VERIFY(
                                qAbs(actions->mapToItem(header, {0, actions->height() / 2}).y() -
                                     (header->property("topPadding").toReal() +
                                      header->property("contentItem")
                                              .value<QQuickItem *>()
                                              ->height() /
                                          2)) < .5);
                            if (!withActions) {
                                QTRY_COMPARE(actions->width(), 32.);
                            } else {
                                auto *refresh = session.item("allocationDrawerRefresh");
                                QVERIFY(refresh);
                                QCOMPARE(refresh->height(), 32.);
                                auto *secondary = session.item("allocationDrawerSecondary");
                                QVERIFY(secondary);
                                QCOMPARE(secondary->isVisible(), loading);
                                auto *spinner = findVisual(refresh, "buttonSpinner");
                                QVERIFY(spinner);
                                QCOMPARE(spinner->isVisible(), loading);
                                if (loading) {
                                    QTRY_VERIFY(qAbs(spinner->mapToItem(refresh, {8, 8}).y() - 16) <
                                                .5);
                                }
                            }
                        }
                    }
                    if (!capture.isEmpty()) {
                        session.window->update();
                        QTest::qWait(60);
                        QVERIFY(session.window->grabWindow().save(
                            capture +
                            QString("/drawer-%1-%2-%3.png").arg(int(mode)).arg(scale).arg(width)));
                    }
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void textTagAlignmentAcrossProductionRecipes()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("TextTagAlignmentHarness.qml")));
        const auto customFamily = QFontDatabase::hasFamily(QStringLiteral("Helvetica"))
                                      ? QStringLiteral("Helvetica")
                                      : QFontDatabase::families().value(0);
        const auto capture = qEnvironmentVariable("ECHO_TEXT_TAG_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        int captured = 0;
        // Pixels, not just Item centers: the previous implementation passed
        // geometry-only checks while the large Chinese title still sat too low.
        auto verifyInk = [&]() -> bool {
            QTest::qWait(60);
            const auto image = session.window->grabWindow();
            if (image.isNull())
                return false;
            const qreal ratio = image.width() / qreal(session.window->width());
            for (int index = 0; index < 5; ++index) {
                auto *row = session.item(QString("alignmentRow-%1").arg(index));
                if (!row)
                    return false;
                auto *label = row->property("textItem").value<QQuickItem *>();
                auto *box = row->property("textBox").value<QQuickItem *>();
                auto *tag = findVisual(row, index < 3 ? "alignmentTag" : "alignmentBadge");
                if (!label || !box || !tag)
                    return false;
                const auto bounds =
                    box->mapRectToScene(QRectF(0, -3, box->width(), box->height() + 6));
                const auto pixels = QRectF(bounds.topLeft() * ratio, bounds.size() * ratio)
                                        .toAlignedRect()
                                        .intersected(image.rect());
                int top = image.height(), bottom = -1;
                for (int y = pixels.top(); y <= pixels.bottom(); ++y) {
                    for (int x = pixels.left(); x <= pixels.right(); ++x) {
                        const auto color = image.pixelColor(x, y);
                        if (color.red() > 120 && color.blue() > 70 &&
                            color.red() > color.green() * 1.8 &&
                            color.blue() > color.green() * 1.4) {
                            top = std::min(top, y);
                            bottom = std::max(bottom, y);
                        }
                    }
                }
                if (bottom < top)
                    return false;
                const qreal actual = (top + bottom + 1) / (2 * ratio);
                const qreal expected = box->mapToScene({0, box->height() / 2}).y();
                const qreal tolerance = 1.0 * session.root->property("sampleScale").toReal();
                if (qAbs(actual - expected) > tolerance) {
                    qWarning() << "ink alignment" << index << label->property("text") << "actual"
                               << actual << "expected" << expected;
                    return false;
                }
                if (tag->isVisible()) {
                    QQuickItem *tagText = nullptr;
                    std::function<void(QQuickItem *)> locate = [&](QQuickItem *item) {
                        if (item->property("font").canConvert<QFont>() &&
                            !item->property("text").toString().isEmpty())
                            tagText = item;
                        for (auto *child : item->childItems())
                            locate(child);
                    };
                    locate(tag);
                    if (!tagText)
                        return false;
                    const QColor foreground = tagText->property("color").value<QColor>();
                    const auto interior =
                        tag->mapRectToScene(QRectF(3, 2, tag->width() - 6, tag->height() - 4));
                    const auto tagPixels =
                        QRectF(interior.topLeft() * ratio, interior.size() * ratio)
                            .toAlignedRect()
                            .intersected(image.rect());
                    int inkTop = image.height(), inkBottom = -1;
                    for (int y = tagPixels.top(); y <= tagPixels.bottom(); ++y) {
                        for (int x = tagPixels.left(); x <= tagPixels.right(); ++x) {
                            const auto pixel = image.pixelColor(x, y);
                            const int distance = qAbs(pixel.red() - foreground.red()) +
                                                 qAbs(pixel.green() - foreground.green()) +
                                                 qAbs(pixel.blue() - foreground.blue());
                            if (distance < 45) {
                                inkTop = std::min(inkTop, y);
                                inkBottom = std::max(inkBottom, y);
                            }
                        }
                    }
                    if (inkBottom < inkTop)
                        return false;
                    const auto center = tag->mapToScene({0, tag->height() / 2}).y();
                    if (qAbs((inkTop + inkBottom + 1) / (2 * ratio) - center) > tolerance)
                        return false;
                    // Inline tags center on rendered text; wrapped groups retain their own line
                    // height.
                    const auto tagTop = tag->mapToScene({0, 0}).y();
                    if (tagTop < box->mapToScene({0, box->height()}).y()) {
                        const auto tagCenter = tag->mapToScene({0, tag->height() / 2}).y();
                        if (qAbs(expected - tagCenter) > tolerance)
                            return false;
                    }
                }
            }
            if (!capture.isEmpty())
                return image.save(capture + QString("/matrix-%1.png").arg(captured++));
            return true;
        };
        for (const auto &family : {QStringLiteral("system-ui"), customFamily}) {
            session.theme.setGlobalFont(family);
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                session.theme.setMode(mode);
                for (const qreal scale : {1.0, 1.25}) {
                    session.root->setProperty("sampleScale", scale);
                    session.root->setProperty("availableWidth", 560);
                    for (const auto &text : {QStringLiteral("一天"), QStringLiteral("EchoMusic"),
                                             QStringLiteral("一天 EchoMusic 123 gyp")}) {
                        session.root->setProperty("sampleText", text);
                        QVERIFY2(verifyInk(), qPrintable(family + " / " + text));
                    }
                }
            }
        }
        session.root->setProperty("sampleScale", 1.0);
        session.root->setProperty("availableWidth", 200);
        session.root->setProperty("sampleText",
                                  QStringLiteral("一天 EchoMusic 一天 EchoMusic 一天"));
        QVERIFY(verifyInk());
        for (const auto index : {0, 4}) {
            auto *row = session.item(QString("alignmentRow-%1").arg(index));
            auto *label = row->property("textItem").value<QQuickItem *>();
            QVERIFY(label->property("lineCount").toInt() > 1);
        }
        session.root->setProperty("showTags", false);
        session.root->setProperty("sampleText", QStringLiteral("一天"));
        QVERIFY(verifyInk());
        auto *row = session.item("alignmentRow-0");
        auto *box = row->property("textBox").value<QQuickItem *>();
        QTRY_COMPARE(row->implicitWidth(), box->width());
        session.root->setProperty("showTags", true);
        QTRY_VERIFY(row->implicitWidth() > box->width() + 8);
        QVERIFY(verifyInk());
        // Flex labels keep their tags at the far edge when shortened, elided,
        // or asynchronously hidden; the other production recipes stay natural.
        auto *flexRow = session.item("alignmentRow-1");
        auto *flexTag = findVisual(flexRow, "alignmentTag");
        auto *flexBox = flexRow->property("textBox").value<QQuickItem *>();
        QVERIFY(flexTag && flexBox);
        flexRow->setProperty("fillTextWidth", true);
        for (const auto text :
             {QStringLiteral("一天"), QStringLiteral("一天 EchoMusic 一天 EchoMusic 一天")}) {
            session.root->setProperty("sampleText", text);
            QTRY_VERIFY(qAbs(flexTag->mapToItem(flexRow, QPointF(flexTag->width(), 0)).x() -
                             flexRow->width()) < .5);
            QVERIFY(verifyInk());
        }
        session.root->setProperty("showTags", false);
        QTRY_COMPARE(flexBox->width(), flexRow->width());
        session.root->setProperty("showTags", true);
        QTRY_VERIFY(qAbs(flexTag->mapToItem(flexRow, QPointF(flexTag->width(), 0)).x() -
                         flexRow->width()) < .5);
        QVERIFY(verifyInk());
        QCOMPARE(warnings.size(), 0);
    }
    void hintsAvoidTriggersAndWindowEdgesAcrossZoom()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        QQmlComponent component(&session.engine);
        component.setData(R"(
import QtQuick
import EchoMusic.Controls
Item {
    id: trigger
    width: 36
    height: 32
    transformOrigin: Item.TopLeft
    property bool requestHint: true
    property string explanation: "不明确的图标：说明操作范围及额外信息 Long explanation"
    Rectangle {
        anchors.fill: parent
        color: "#888888"
    }
    EchoToolTip {
        objectName: "placementHint"
        parent: trigger
        delay: 0
        requestedVisible: trigger.requestHint
        text: trigger.explanation
    }
})",
                          QUrl("tooltip-placement.qml"));
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *trigger = qobject_cast<QQuickItem *>(root.data());
        QVERIFY(trigger);
        trigger->setParentItem(session.window->contentItem());
        auto *tip = root->findChild<QObject *>("placementHint");
        QVERIFY(tip);
        const auto capture = qEnvironmentVariable("ECHO_TOOLTIP_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const double zoom : {1., 1.25}) {
                session.window->setProperty("uiScale", zoom);
                trigger->setScale(zoom);
                for (const auto position :
                     {QPointF(0, 0), QPointF(720 - 36 * zoom, 0), QPointF(0, 290 - 32 * zoom),
                      QPointF(720 - 36 * zoom, 290 - 32 * zoom), QPointF(350, 130)}) {
                    trigger->setPosition(position);
                    QTRY_VERIFY(tip->property("visible").toBool());
                    QTRY_COMPARE(
                        tip->property("popup").value<QObject *>()->property("scale").toReal(),
                        zoom);
                    auto *content = tip->property("popup")
                                        .value<QObject *>()
                                        ->property("contentItem")
                                        .value<QQuickItem *>();
                    QVERIFY(content && content->parentItem());
                    const auto positioned = [&] {
                        const auto painted = content->parentItem()->mapRectToScene(
                            content->parentItem()->boundingRect());
                        const auto anchor = trigger->mapRectToScene(trigger->boundingRect());
                        const QRectF usable(8 * zoom - .6, 8 * zoom - .6, 720 - 16 * zoom + 1.2,
                                            290 - 16 * zoom + 1.2);
                        return usable.contains(painted) && !painted.intersects(anchor);
                    };
                    QTRY_VERIFY(positioned());
                    QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                    session.window->update();
                    QTRY_VERIFY(!frames.isEmpty());
                    if (!capture.isEmpty())
                        QVERIFY(session.window->grabWindow().save(capture +
                                                                  QString("/hint-%1-%2-%3-%4.png")
                                                                      .arg(int(mode))
                                                                      .arg(zoom)
                                                                      .arg(position.x())
                                                                      .arg(position.y())));
                }
                // A hovered icon grows visually; the shared hint keeps the
                // window's text scale while tracking that animated trigger.
                trigger->setScale(zoom * 1.1);
                QTRY_COMPARE(tip->property("popup").value<QObject *>()->property("scale").toReal(),
                             zoom);
                trigger->setScale(zoom);
                // Dense explanations remain bounded and cannot cover the trigger.
                trigger->setProperty("explanation", QStringLiteral("长说明 Long ").repeated(100));
                QTRY_VERIFY(tip->property("visible").toBool());
                auto *content = tip->property("popup")
                                    .value<QObject *>()
                                    ->property("contentItem")
                                    .value<QQuickItem *>();
                QTRY_VERIFY(content->property("truncated").toBool());
                const auto painted =
                    content->parentItem()->mapRectToScene(content->parentItem()->boundingRect());
                QVERIFY(!painted.intersects(trigger->mapRectToScene(trigger->boundingRect())));
                trigger->setProperty("explanation", QStringLiteral("必要说明 Long explanation"));
            }
        }
        session.window->resize(220, 180);
        trigger->setPosition({175, 0});
        trigger->setProperty("explanation",
                             QStringLiteral("窄窗必要说明 Long explanation ").repeated(4));
        const auto fitsNarrowWindow = [&] {
            auto *content = tip->property("popup")
                                .value<QObject *>()
                                ->property("contentItem")
                                .value<QQuickItem *>();
            const auto rect =
                content->parentItem()->mapRectToScene(content->parentItem()->boundingRect());
            return QRectF(9.4, 9.4, 201.2, 161.2).contains(rect) &&
                   !rect.intersects(trigger->mapRectToScene(trigger->boundingRect()));
        };
        QTRY_VERIFY(fitsNarrowWindow());
        if (!capture.isEmpty())
            QVERIFY(session.window->grabWindow().save(capture + "/hint-narrow.png"));
        session.window->resize(720, 290);
        trigger->setPosition({350, 130});
        QScopedPointer<QObject> secondRoot(component.create());
        QVERIFY(secondRoot);
        auto *secondTrigger = qobject_cast<QQuickItem *>(secondRoot.data());
        secondTrigger->setPosition({350, 30});
        secondTrigger->setParentItem(session.window->contentItem());
        auto *secondTip = secondRoot->findChild<QObject *>("placementHint");
        QVERIFY(secondTip);
        QTRY_VERIFY(secondTip->property("visible").toBool());
        QTRY_VERIFY(!tip->property("visible").toBool());
        secondTrigger->setProperty("requestHint", false);
        QTRY_VERIFY(!secondTip->property("visible").toBool());
        QTest::qWait(550);
        QVERIFY(!tip->property("visible").toBool());
        trigger->setProperty("requestHint", false);
        trigger->setProperty("requestHint", true);
        QTRY_VERIFY(tip->property("visible").toBool());
        trigger->setProperty("requestHint", false);
        QTRY_VERIFY(!tip->property("visible").toBool());
        QCOMPARE(warnings.size(), 0);
    }
    void popupsCancelVisibleAndPendingHints()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        // A pinned player from another app can cover Cocoa's default centered
        // test window and cancel native hover after QTest injects its move.
        // Keep the real pointer and trigger in an exposed part of the work area.
        session.window->setPosition(session.window->screen()->availableGeometry().topLeft() +
                                    QPoint(16, 16));
        QVERIFY(Test::prepareNativeInput(session.window));
        auto *button = session.item("iconButton");
        auto *popup = session.root->findChild<QObject *>("tooltipTestPopup");
        QVERIFY(button && popup);
        QVERIFY(!button->findChild<QObject *>("iconButtonToolTip"));
        button->setProperty("tooltipText", QStringLiteral("Favorite explanation"));
        auto *tip = button->findChild<QObject *>("iconButtonToolTip");
        QVERIFY(tip);
        auto hover = [&] {
            QVERIFY(Test::prepareNativeInput(session.window));
            Test::moveNativePointer(session.window, {10, 270});
            QTRY_VERIFY(!tip->property("requestedVisible").toBool());
            // Drain Cocoa's popup grab/enter-leave events before the next real
            // hover. QTest's injected move alone does not settle that queue.
            QTest::qWait(80);
            Test::moveNativePointer(
                session.window,
                button->mapToScene({button->width() / 2, button->height() / 2}).toPoint());
            QTRY_VERIFY(tip->property("requestedVisible").toBool());
        };
        hover();
        QVERIFY(!tip->property("popup").value<QObject *>());
        QTRY_VERIFY2(
            tip->property("visible").toBool(),
            qPrintable(
                QString("requested=%1 dismissed=%2 eligible=%3 has-popup=%4 hover=%5 active=%6")
                    .arg(tip->property("requestedVisible").toBool())
                    .arg(tip->property("dismissedUntilHoverEnds").toBool())
                    .arg(tip->property("eligible").toBool())
                    .arg(tip->property("popup").value<QObject *>() != nullptr)
                    .arg(button->property("hovered").toBool())
                    .arg(session.window->isActive())));
        QPointer<QObject> visibleNativeHint = tip->property("popup").value<QObject *>();
        QVERIFY(visibleNativeHint);
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("opened").toBool());
        QTRY_VERIFY(!tip->property("visible").toBool());
        QTRY_VERIFY(visibleNativeHint.isNull());
        QTest::qWait(600);
        QVERIFY(!tip->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        QTRY_VERIFY(!popup->property("visible").toBool());
        QVERIFY(!tip->property("visible").toBool());
        hover();
        // The new hover has requested a hint but its delay has not elapsed.
        QVERIFY(!tip->property("visible").toBool());
        QVERIFY(!tip->property("popup").value<QObject *>());
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("opened").toBool());
        QTest::qWait(600);
        QVERIFY(!tip->property("visible").toBool());
        QVERIFY(!tip->property("popup").value<QObject *>());
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        QTRY_VERIFY(!popup->property("visible").toBool());
        hover();
        QTRY_VERIFY2(tip->property("visible").toBool(),
                     qPrintable(QString("requested=%1 dismissed=%2 eligible=%3 hover=%4 "
                                        "cursor=%5,%6 target=%7,%8 active=%9")
                                    .arg(tip->property("requestedVisible").toBool())
                                    .arg(tip->property("dismissedUntilHoverEnds").toBool())
                                    .arg(tip->property("eligible").toBool())
                                    .arg(button->property("hovered").toBool())
                                    .arg(QCursor::pos().x())
                                    .arg(QCursor::pos().y())
                                    .arg(button->mapToGlobal({20, 18}).x())
                                    .arg(button->mapToGlobal({20, 18}).y())
                                    .arg(session.window->isActive())));
        QPointer<QObject> ownedHint = tip;
        button->setProperty("tooltipText", QString());
        QTRY_VERIFY(ownedHint.isNull());
        QCOMPARE(warnings.size(), 0);
    }
    void badgesAndTagsKeepDistinctContracts()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("BadgeTagHarness.qml")));
        auto *badge = session.item("countBadge");
        auto *tag = session.item("statusTag");
        auto *floating = session.item("floatingCount");
        auto *owner = session.item("badgeOwner");
        auto *content = session.item("contentTag");
        QVERIFY(badge && tag && floating && owner && content);
        // Zero is a real count; status text has an independent public contract.
        QVERIFY(badge->isVisible());
        QCOMPARE(badge->property("count").toInt(), 0);
        QVERIFY(badge->metaObject()->indexOfProperty("text") < 0);
        QVERIFY(tag->metaObject()->indexOfProperty("count") < 0);
        QTRY_COMPARE(badge->size(), tag->size());
        QCOMPARE(badge->height(), 18.);
        tag->setProperty("text", QStringLiteral("当前登录"));
        QTRY_VERIFY(tag->width() > badge->width());
        badge->setProperty("count", QVariant());
        QTRY_VERIFY(!badge->isVisible());
        QVERIFY(tag->isVisible());
        badge->setProperty("count", QStringLiteral("99+"));
        QTRY_VERIFY(badge->isVisible());
        QVERIFY(floating->x() + floating->width() > owner->width());
        QVERIFY(floating->y() < 0);
        const auto originalX = floating->x();
        owner->setWidth(owner->width() + 50);
        QTRY_COMPARE(floating->x(), originalX + 50);
        QTRY_COMPARE(content->width(), 50.);
        QCOMPARE(content->height(), 18.);
        QCOMPARE(warnings.size(), 0);
    }
    void globalTypographyRetainsFallbackAndLocalStyles()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("TypographyHarness.qml")));
        const auto initialFamily = session.theme.applicationFont().families().first();
        const auto selected = QFontDatabase::hasFamily(QStringLiteral("Helvetica"))
                                  ? QStringLiteral("Helvetica")
                                  : QFontDatabase::families().value(0);
        QVERIFY(!selected.isEmpty());
        const QList<QPair<QString, int>> samples{{"weight400", 400},
                                                 {"weight500", 500},
                                                 {"weight600", 600},
                                                 {"weight650", 650},
                                                 {"weight700", 700},
                                                 {"weight800", 800},
                                                 {"weight900", 900},
                                                 {"typographySoftButton", 600},
                                                 {"typographyFormButton", 900},
                                                 {"typographyInput", 700},
                                                 {"typographyButtonContent", 600},
                                                 {"typographyDecoratedText", 600}};
        // Changing preferences must update already-created controls, including
        // overrides on derived controls, without rebuilding the window.
        for (const auto &preference :
             {QStringLiteral("system-ui"), selected, QStringLiteral("system-ui")}) {
            session.theme.setGlobalFont(preference);
            for (const auto &[name, weight] : samples) {
                auto *item = session.item(name);
                QVERIFY2(item, qPrintable(name));
                QTRY_COMPARE(item->property("font").value<QFont>().families(),
                             session.theme.applicationFont().families());
                const auto font = item->property("font").value<QFont>();
                QCOMPARE(font.weight(), weight);
                if (name == "typographySoftButton" || name == "typographyInput")
                    QCOMPARE(font.pixelSize(), 14);
                else if (name == "typographyFormButton")
                    QCOMPARE(font.pixelSize(), 16);
                else if (name.startsWith("weight")) {
                    QCOMPARE(font.pixelSize(), 20);
                    // QFont stores spacing on a 1/64px fixed-point grid.
                    QVERIFY(qAbs(font.letterSpacing() + 0.2) <= 1.0 / 64);
                }
                if (name == "typographyDecoratedText") {
                    QVERIFY(font.italic());
                    QVERIFY(font.underline());
                    QCOMPARE(font.letterSpacing(), 0.5);
                }
#ifdef Q_OS_MACOS
                if (weight > 400 && QFontDatabase::hasFamily(QStringLiteral("PingFang SC"))) {
                    auto regularFont = font;
                    regularFont.setWeight(QFont::Normal);
                    QTextLayout latinNormal("Y", regularFont), latinStyled("Y", font);
                    for (auto *layout : {&latinNormal, &latinStyled}) {
                        layout->beginLayout();
                        layout->createLine();
                        layout->endLayout();
                    }
                    const auto latinRun = latinStyled.glyphRuns().first();
                    const auto latinRegularRun = latinNormal.glyphRuns().first();
                    const bool hasLatinVariant =
                        latinRun.rawFont().pathForGlyph(latinRun.glyphIndexes().first()) !=
                        latinRegularRun.rawFont().pathForGlyph(
                            latinRegularRun.glyphIndexes().first());
                    // Numeric QFont/QRawFont weights can report 600 while the
                    // actual Chinese face remains Regular. Check glyph outlines.
                    for (const auto &text :
                         {QStringLiteral("Y"), QStringLiteral("茶"), QStringLiteral("《")}) {
                        auto regular = font;
                        regular.setWeight(QFont::Normal);
                        QTextLayout normal(text, regular), styled(text, font);
                        for (auto *layout : {&normal, &styled}) {
                            layout->beginLayout();
                            layout->createLine();
                            layout->endLayout();
                        }
                        QCOMPARE(normal.glyphRuns().size(), 1);
                        QCOMPARE(styled.glyphRuns().size(), 1);
                        const auto run = styled.glyphRuns().first();
                        const auto normalRun = normal.glyphRuns().first();
                        QCOMPARE(run.glyphIndexes().size(), 1);
                        QVERIFY(run.glyphIndexes().first() != 0);
                        const auto outline = run.rawFont().pathForGlyph(run.glyphIndexes().first());
                        QVERIFY(!outline.isEmpty());
                        if ((name == "weight500" || name == "weight600") &&
                            preference == "system-ui")
                            qInfo().noquote() << name << text << run.rawFont().familyName()
                                              << run.rawFont().styleName();
                        // Some installed families (e.g. Helvetica at 500) have
                        // no Medium variant; Qt resolves both scripts to Regular.
                        // Require heavier CJK when Latin is heavier, and real
                        // variants for the Cocoa system-ui baseline.
                        if (!hasLatinVariant && (QGuiApplication::platformName() != "cocoa" ||
                                                 preference != "system-ui"))
                            continue;
                        QVERIFY2(outline != normalRun.rawFont().pathForGlyph(
                                                normalRun.glyphIndexes().first()),
                                 qPrintable(name + ": " + text + " under " + preference + " uses " +
                                            run.rawFont().familyName() + " " +
                                            run.rawFont().styleName() +
                                            " instead of a heavier outline than " +
                                            normalRun.rawFont().familyName() + " " +
                                            normalRun.rawFont().styleName()));
                    }
                }
#endif
            }
        }
        QCOMPARE(session.theme.applicationFont().families().first(), initialFamily);
        const auto capture = qEnvironmentVariable("ECHO_TYPOGRAPHY_CAPTURE_DIR");
        if (!capture.isEmpty() && QGuiApplication::platformName() != "offscreen") {
            QVERIFY(QDir().mkpath(capture));
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                session.theme.setMode(mode);
                QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                session.window->update();
                QTRY_VERIFY(!frames.isEmpty());
                const auto image = session.window->grabWindow();
                QVERIFY(!image.isNull());
                QVERIFY(
                    image.save(capture + (mode == ThemeStore::Dark ? "/dark.png" : "/light.png")));
            }
        }
        QCOMPARE(warnings.count(), 0);
    }

    void floatingBorderCompositesOverItsOwnFill()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        session.window->setHeight(440);
        session.root->setProperty("surfaceProbe", true);
        auto *surface = session.item("borderProbeSurface");
        QVERIFY(surface);
        for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
            session.theme.setMode(mode);
            for (const qreal scale : {1.0, 1.25, 1.5}) {
                session.root->setProperty("surfaceScale", scale);
                // An offscreen window has no native RHI framebuffer to sample;
                // keep the real pixel assertions in the Cocoa/device run.
                if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
                    continue;
                QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                session.window->update();
                QTRY_VERIFY(!frames.isEmpty());
                const auto image = session.window->grabWindow();
                QVERIFY(!image.isNull());
                const auto dpr = session.window->devicePixelRatio();
                const auto pixel = [&](const QPointF &local) {
                    const auto point = surface->mapToScene(local) * dpr;
                    return image.pixelColor(qFloor(point.x()), qFloor(point.y()));
                };
                const auto fill = session.theme.elevated(), tone = session.theme.floatingTone();
                const QColor expected(qRound(fill.red() * 0.9 + tone.red() * 0.1),
                                      qRound(fill.green() * 0.9 + tone.green() * 0.1),
                                      qRound(fill.blue() * 0.9 + tone.blue() * 0.1));
                // The two straight-edge points cover different saturated
                // backgrounds. CSS paints the surface under its border too.
                for (const qreal y : {60.0, 180.0}) {
                    const auto edge = pixel({0.5, y}), interior = pixel({3, y});
                    QVERIFY2(qAbs(edge.red() - expected.red()) <= 3 &&
                                 qAbs(edge.green() - expected.green()) <= 3 &&
                                 qAbs(edge.blue() - expected.blue()) <= 3,
                             "Border must composite over the panel fill, not the card behind it");
                    QVERIFY(qAbs(interior.red() - fill.red()) <= 2 &&
                            qAbs(interior.green() - fill.green()) <= 2 &&
                            qAbs(interior.blue() - fill.blue()) <= 2);
                }
                const auto directory = qEnvironmentVariable("ECHO_CONTROLS_CAPTURE_DIR");
                if (!directory.isEmpty()) {
                    QVERIFY(QDir().mkpath(directory));
                    QVERIFY(
                        image.save(directory + QString("/floating-border-%1-%2.png")
                                                   .arg(mode == ThemeStore::Dark ? "dark" : "light")
                                                   .arg(scale)));
                }
            }
        }
        QCOMPARE(warnings.count(), 0);
    }
    void popupAndPageTracksShareRightInsetAndRetainDragMapping()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create(QStringLiteral("PopupScrollHarness.qml")));
        auto *panel = session.root->property("panel").value<QObject *>();
        QVERIFY(panel);
        QVERIFY(QMetaObject::invokeMethod(panel, "open"));
        QTRY_VERIFY(panel->property("opened").toBool());
        auto *page = session.item("pageTrack");
        QVERIFY(page);
        auto *pageThumb = findVisual(page, "scrollThumb");
        QVERIFY(pageThumb);
        const auto pageInset = [&] {
            return page->parentItem()->width() -
                   pageThumb->mapToItem(page->parentItem(), {pageThumb->width(), 0}).x();
        };
        for (int kind = 0; kind < 3; ++kind) {
            QPointer<QQuickItem> previous = kind > 0 ? session.item("popupTrack") : nullptr;
            session.root->setProperty("viewKind", kind);
            if (kind > 0)
                QTRY_VERIFY(previous.isNull());
            QQuickItem *track = nullptr;
            QTRY_VERIFY((track = session.item("popupTrack")));
            auto *thumb = findVisual(track, "scrollThumb");
            QVERIFY(thumb);
            auto *content = panel->property("contentItem").value<QQuickItem *>();
            QVERIFY(content);
            QCOMPARE(track->parentItem(), content->parentItem());
            for (const int padding : {0, 8, 24})
                for (const qreal scale : {0.8, 1.0, 1.5}) {
                    session.root->setProperty("contentPadding", padding);
                    session.root->setProperty("surfaceScale", scale);
                    QTRY_VERIFY(track->height() > 0);
                    QTRY_COMPARE(track->x() + track->width(), qreal(260));
                    QTRY_VERIFY(
                        qAbs(track->parentItem()->width() -
                             thumb->mapToItem(track->parentItem(), {thumb->width(), 0}).x() -
                             pageInset()) < 0.01);
                    // The track is outside the clipped/padded Loader, and maps the
                    // scrolling region's vertical bounds through resize and zoom.
                    QTRY_VERIFY_WITH_TIMEOUT(qAbs(track->y() - padding) < 0.01, 1000);
                    QTRY_VERIFY_WITH_TIMEOUT(qAbs(track->height() - (200 - 2 * padding)) < 0.01,
                                             1000);
                }
            auto *view = session.root->property("scrollingView").value<QQuickItem *>();
            QVERIFY(view);
            auto *flick = kind == 2 ? view->property("contentItem").value<QQuickItem *>() : view;
            QVERIFY(flick);
            QTRY_VERIFY(track->property("size").toReal() < 1);
            QTest::qWait(40); // Complete the final layout before native hit testing.
            const auto from =
                thumb->mapToScene({thumb->width() / 2, thumb->height() / 2}).toPoint();
            const auto to = track->mapToScene({track->width() / 2, track->height() - 10}).toPoint();
            QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, from);
            QTest::mouseMove(session.window, to, 20);
            QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, to);
            QTRY_VERIFY(flick->property("contentY").toReal() > 0);
        }
        QPointer<QQuickItem> closingTrack = session.item("popupTrack");
        QVERIFY(closingTrack);
        QVERIFY(QMetaObject::invokeMethod(panel, "close"));
        QTRY_VERIFY(!closingTrack || !closingTrack->isVisible());
        // Exercise the actual shared Select/Menu too: each has its own popup
        // content model and padding, rather than the generic fixture viewport.
        for (const auto *name : {"selectPanel", "contextMenu"}) {
            if (QByteArray(name) == "selectPanel")
                QVERIFY(QMetaObject::invokeMethod(session.root.data(), "openSelect"));
            auto *actual = session.root->property(name).value<QObject *>();
            QVERIFY(actual);
            QVERIFY(QMetaObject::invokeMethod(actual, "open"));
            QTRY_VERIFY(actual->property("opened").toBool());
            QQuickItem *track = nullptr;
            std::function<QQuickItem *(QQuickItem *)> find = [&](QQuickItem *node) -> QQuickItem * {
                if (node->objectName() == "popupScrollBar" &&
                    node->property("surface").value<QObject *>() == actual)
                    return node;
                for (auto *child : node->childItems())
                    if (auto *found = find(child))
                        return found;
                return nullptr;
            };
            QTRY_VERIFY((track = find(session.window->contentItem())));
            QTRY_VERIFY(track->property("size").toReal() < 1);
            QTRY_COMPARE(track->x() + track->width(), actual->property("width").toReal());
            QPointer<QQuickItem> closing = track;
            QVERIFY(QMetaObject::invokeMethod(actual, "close"));
            QTRY_VERIFY(!closing || !closing->isVisible());
        }
        QCOMPARE(warnings.count(), 0);
    }
    void centeredContentAcrossSizesAndPadding()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        QVERIFY(session.engine.imageProvider("echo-icons"));
        for (const int height : {32, 36, 48})
            for (const int fontSize : {12, 20})
                for (const int padding : {0, 6, 12}) {
                    session.root->setProperty("buttonHeight", height);
                    session.root->setProperty("labelSize", fontSize);
                    session.root->setProperty("verticalPadding", padding);
                    for (const auto *name : {"accentButton", "neutralButton", "ordinaryButton"}) {
                        auto *button = session.item(name);
                        auto *glyph = findVisual(button, "buttonGlyph");
                        auto *label = findVisual(button, "buttonLabel");
                        QVERIFY(glyph && label);
                        QTRY_VERIFY(qAbs(glyph
                                             ->mapToItem(button, QPointF(glyph->width() / 2,
                                                                         glyph->height() / 2))
                                             .y() -
                                         button->height() / 2) < 0.6);
                        QTRY_VERIFY(qAbs(label
                                             ->mapToItem(button, QPointF(label->width() / 2,
                                                                         label->height() / 2))
                                             .y() -
                                         button->height() / 2) < 0.6);
                        QCOMPARE(glyph->property("color").value<QColor>(),
                                 label->property("color").value<QColor>());
                    }
                }
        // The locked Qt SDK exports RowReverse (the online preview docs use a
        // different spelling). Exercise the real reversed-content path used by
        // account actions so an unresolved enum cannot silently keep Row order.
        for (const auto *name : {"accentButton", "neutralButton", "ordinaryButton"}) {
            auto *button = session.item(name);
            auto *content = button->property("contentItem").value<QQuickItem *>();
            auto *glyph = findVisual(button, "buttonGlyph");
            auto *label = findVisual(button, "buttonLabel");
            QVERIFY(content && glyph && label);
            content->setProperty("glyphAfterText", true);
            QTRY_VERIFY(glyph->mapToItem(content, QPointF()).x() >=
                        label->mapToItem(content, QPointF(label->width(), 0)).x());
            content->setProperty("glyphAfterText", false);
            QTRY_VERIFY(label->mapToItem(content, QPointF()).x() >=
                        glyph->mapToItem(content, QPointF(glyph->width(), 0)).x());
        }
        session.root->setProperty("label", QStringLiteral("中文与 Latin 长按钮文字，必须省略"));
        auto *label = findVisual(session.item("accentButton"), "buttonLabel");
        QTRY_VERIFY(label->property("truncated").toBool());
        QCOMPARE(warnings.count(), 0);
    }
    void iconTextContentCentersRenderedInk()
    {
        ControlsSession session;
        QVERIFY(session.create());
        session.window->resize(900, 400);
        session.window->contentItem()->setTransformOrigin(QQuickItem::TopLeft);
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        const auto capture = qEnvironmentVariable("ECHO_ICON_TEXT_CAPTURE_DIR");
        if (!capture.isEmpty())
            QVERIFY(QDir().mkpath(capture));
        const auto customFamily = QFontDatabase::hasFamily(QStringLiteral("Helvetica"))
                                      ? QStringLiteral("Helvetica")
                                      : QFontDatabase::families().value(0);
        int captured = 0;
        session.root->setProperty("buttonHeight", 38);
        session.root->setProperty("labelSize", 13);
        // Check rendered Chinese/fallback/Latin ink against the icon box, not
        // just Text's ascent rectangle. That geometry passed before this fix.
        for (const auto &family : {QStringLiteral("system-ui"), customFamily}) {
            session.theme.setGlobalFont(family);
            for (const auto mode : {ThemeStore::Light, ThemeStore::Dark}) {
                session.theme.setMode(mode);
                for (const qreal scale : {1.0, 1.25}) {
                    session.window->contentItem()->setScale(scale);
                    for (const auto &text :
                         {QStringLiteral("任务中心"), QStringLiteral("更多应用 EchoMusic gyp")}) {
                        QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                        session.root->setProperty("label", text);
                        session.window->requestUpdate();
                        QTRY_VERIFY(frames.size() > 0);
                        const auto image = session.window->grabWindow();
                        QVERIFY(!image.isNull());
                        if (!capture.isEmpty())
                            QVERIFY(
                                image.save(capture + QString("/buttons-%1.png").arg(captured++)));
                        const auto ratio = image.width() / qreal(session.window->width());
                        for (const auto *name :
                             {"accentButton", "neutralButton", "ordinaryButton"}) {
                            auto *button = session.item(name);
                            auto *label = findVisual(button, "buttonLabel");
                            auto *glyph = findVisual(button, "buttonGlyph");
                            auto *box = label->property("textBox").value<QQuickItem *>();
                            QVERIFY(box && glyph);
                            const auto foreground = label->property("color").value<QColor>();
                            const auto rect =
                                box->mapRectToScene(QRectF(0, -2, box->width(), box->height() + 4));
                            const auto pixels = QRectF(rect.topLeft() * ratio, rect.size() * ratio)
                                                    .toAlignedRect()
                                                    .intersected(image.rect());
                            int top = image.height(), bottom = -1;
                            for (int y = pixels.top(); y <= pixels.bottom(); ++y)
                                for (int x = pixels.left(); x <= pixels.right(); ++x) {
                                    const auto color = image.pixelColor(x, y);
                                    if (qAbs(color.red() - foreground.red()) +
                                            qAbs(color.green() - foreground.green()) +
                                            qAbs(color.blue() - foreground.blue()) <
                                        80) {
                                        top = std::min(top, y);
                                        bottom = std::max(bottom, y);
                                    }
                                }
                            QVERIFY2(
                                bottom >= top,
                                qPrintable(
                                    QString("%1 %2 %3 scale=%4 pixels=%5,%6,%7,%8 labelWidth=%9 "
                                            "color=%10")
                                        .arg(name, family, text)
                                        .arg(scale)
                                        .arg(pixels.x())
                                        .arg(pixels.y())
                                        .arg(pixels.width())
                                        .arg(pixels.height())
                                        .arg(label->width())
                                        .arg(label->property("color").value<QColor>().name())));
                            const auto center =
                                glyph->mapToScene(QPointF(0, glyph->height() / 2)).y();
                            QVERIFY2(
                                qAbs((top + bottom + 1) / (2 * ratio) - center) <= scale,
                                qPrintable(
                                    QString("%1 %2 %3 scale=%4 ink=%5 icon=%6 top=%7 bottom=%8")
                                        .arg(name, family, text)
                                        .arg(scale)
                                        .arg((top + bottom + 1) / (2 * ratio))
                                        .arg(center)
                                        .arg(top)
                                        .arg(bottom)));
                        }
                    }
                }
            }
        }
        QCOMPARE(warnings.size(), 0);
    }
    void allBundledGlyphsTintAndPreserveAlpha()
    {
        ControlsSession session; // Load production module resources and provider.
        QVERIFY(session.create());
        EchoIconImageProvider provider;
        const auto files =
            QDir(":/qt/qml/EchoMusic/Controls/icons").entryList({"*.svg"}, QDir::Files);
        const auto sourceFiles = QDir(ECHO_ICON_SOURCE_DIR).entryList({"*.svg"}, QDir::Files);
        QVERIFY(!sourceFiles.isEmpty());
        // Check the actual packaging inventory rather than freezing a historical
        // count; every shipped glyph below still has tint/alpha assertions.
        QCOMPARE(files, sourceFiles);
        QVERIFY(files.contains("check.svg"));
        const QColor tint(40, 160, 210, 166);
        for (const auto &file : files) {
            QSize original;
            const auto id = file.chopped(4) + "/" + QString::number(tint.rgba(), 16);
            const auto image = provider.requestImage(id, &original, QSize(40, 40));
            QVERIFY2(!image.isNull(), qPrintable(file));
            QCOMPARE(original, file == "verification-spinner.svg" ? QSize(28, 28) : QSize(24, 24));
            QCOMPARE(image.size(), QSize(40, 40));
            QColor strongest(Qt::transparent);
            int transparent = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const auto pixel = image.pixelColor(x, y);
                    if (pixel.alpha() > strongest.alpha())
                        strongest = pixel;
                    transparent += pixel.alpha() == 0;
                }
            QVERIFY(transparent > 0);
            QVERIFY2(qAbs(strongest.alpha() - tint.alpha()) <= 1,
                     qPrintable(file + " alpha=" + QString::number(strongest.alpha())));
            QVERIFY(qAbs(strongest.red() - tint.red()) <= 1);
            QVERIFY(qAbs(strongest.green() - tint.green()) <= 1);
            QVERIFY(qAbs(strongest.blue() - tint.blue()) <= 1);
        }
        QVERIFY(provider.requestImage("../music/ffffffff", nullptr, QSize(40, 40)).isNull());
        QVERIFY(provider.requestImage("music/invalid", nullptr, QSize(40, 40)).isNull());
    }
    void themeCustomColorHoverAndRenderedPixels()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        auto *accent = session.item("accentButton");
        auto *glyph = findVisual(accent, "buttonGlyph");
        auto *iconButton = session.item("iconButton");
        auto *tinted = session.item("tintGlyph");
        auto *fixed = session.item("fixedGlyph");
        QVERIFY(glyph && iconButton && tinted && fixed);
        const auto artifacts = qEnvironmentVariable("ECHO_CONTROLS_REPORT_DIR");
        for (auto mode : {ThemeStore::Mode::Light, ThemeStore::Mode::Dark}) {
            session.theme.setMode(mode);
            session.theme.setAccent(QColor("#bc4477"));
            QTRY_COMPARE(glyph->property("color").value<QColor>(), session.theme.accentText());
            QCOMPARE(fixed->property("color").value<QColor>(), QColor(Qt::black));
            for (QColor tint : {QColor("#cc3576"), QColor("#318adb"), QColor(40, 160, 210, 166)}) {
                session.root->setProperty("explicitColor", tint);
                QTRY_COMPARE(tinted->property("status").toInt(), 1);
                QCOMPARE(tinted->property("color").value<QColor>(), tint);
                if (QGuiApplication::platformName() == "offscreen")
                    continue;
                QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                session.window->update();
                QTRY_VERIFY(!frames.isEmpty());
                const auto image = session.window->grabWindow();
                QVERIFY(!image.isNull());
                const auto scene = tinted->mapToScene(QPointF());
                const auto dpr = session.window->devicePixelRatio();
                const QRect area(int(scene.x() * dpr), int(scene.y() * dpr),
                                 int(tinted->width() * dpr), int(tinted->height() * dpr));
                const auto background = session.theme.main();
                const QColor expected(
                    qRound(tint.red() * tint.alphaF() + background.red() * (1 - tint.alphaF())),
                    qRound(tint.green() * tint.alphaF() + background.green() * (1 - tint.alphaF())),
                    qRound(tint.blue() * tint.alphaF() + background.blue() * (1 - tint.alphaF())));
                int hits = 0;
                for (int y = area.top(); y <= area.bottom(); ++y)
                    for (int x = area.left(); x <= area.right(); ++x) {
                        const auto pixel = image.pixelColor(x, y);
                        hits += qAbs(pixel.red() - expected.red()) <= 3 &&
                                qAbs(pixel.green() - expected.green()) <= 3 &&
                                qAbs(pixel.blue() - expected.blue()) <= 3;
                    }
                QVERIFY2(hits > 8, "Rendered icon must have the requested color, not merely a "
                                   "matching QML property");
                if (!artifacts.isEmpty()) {
                    QVERIFY(QDir().mkpath(artifacts));
                    QVERIFY(image.save(artifacts + (mode == ThemeStore::Mode::Dark
                                                        ? "/controls-dark.png"
                                                        : "/controls-light.png")));
                }
            }
        }
        Test::moveNativePointer(
            session.window,
            iconButton->mapToScene(QPointF(iconButton->width() / 2, iconButton->height() / 2))
                .toPoint());
        QTRY_VERIFY(iconButton->property("hovered").toBool());
        QCOMPARE(iconButton->property("foreground").value<QColor>(), session.theme.text());
        iconButton->setProperty("enabled", false);
        QCOMPARE(warnings.count(), 0);
    }
    void sourceActionSurfacesAndSelectStates()
    {
        ControlsSession session;
        QSignalSpy warnings(&session.engine, &QQmlEngine::warnings);
        QVERIFY(session.create());
        auto *neutral = session.item("neutralButton");
        auto *accent = session.item("accentButton");
        auto *select = session.item("styledSelect");
        auto *danger = session.item("dangerButton");
        QVERIFY(neutral && accent && select && danger);
        auto background = [](QQuickItem *control) {
            return control->property("background").value<QQuickItem *>();
        };
        const auto color = [&](QQuickItem *control) {
            return background(control)->property("color").value<QColor>();
        };
        auto alpha = [](QColor value, qreal amount) {
            value.setAlphaF(amount);
            return value;
        };
        for (auto mode : {ThemeStore::Light, ThemeStore::Dark})
            for (bool floating : {false, true}) {
                session.theme.setMode(mode);
                session.theme.selectSolid(QColor("#7da3ce"));
                session.theme.setAccent(QColor("#bc4477"));
                session.root->setProperty("floatingControls", floating);
                const auto tone = floating ? session.theme.floatingTone() : session.theme.tone();
                const auto mouse = [&](QQuickItem *item) {
                    Test::moveNativePointer(
                        session.window,
                        item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
                };
                Test::moveNativePointer(session.window, QPoint(700, 250));
                QTRY_COMPARE(color(danger), alpha(QColor("#ef4444"), 0.08));
                mouse(danger);
                QTRY_COMPARE(color(danger), alpha(QColor("#ef4444"), 0.12));
                const auto dangerPoint =
                    danger->mapToScene(QPointF(danger->width() / 2, danger->height() / 2))
                        .toPoint();
                QVERIFY(Test::prepareNativeInput(session.window));
                QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, dangerPoint);
                QTRY_VERIFY(danger->property("down").toBool());
                QTRY_COMPARE(color(danger), alpha(QColor("#ef4444"), 0.16));
                QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, dangerPoint);
                Test::moveNativePointer(session.window, QPoint(700, 250));
                QTRY_COMPARE(color(neutral), alpha(tone, 0.05));
                QCOMPARE(background(neutral)->property("radius").toReal(), 6.0);
                QCOMPARE(select->implicitHeight(), 36.0);
                QCOMPARE(background(select)->property("radius").toReal(), 6.0);
                // The actual render must preserve the 5% translucent fill: a shadow
                // painted inside would silently add another 6% black over it.
                if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
                    QSignalSpy frames(session.window, &QQuickWindow::frameSwapped);
                    session.window->update();
                    QTRY_VERIFY(!frames.isEmpty());
                    const auto image = session.window->grabWindow();
                    const auto dpr = session.window->devicePixelRatio();
                    const auto point =
                        neutral->mapToScene(QPointF(neutral->width() / 2, neutral->height() - 7));
                    const auto pixel =
                        image.pixelColor(qRound(point.x() * dpr), qRound(point.y() * dpr));
                    const auto base = floating ? session.theme.elevated() : session.theme.main();
                    const auto fill = alpha(tone, 0.05);
                    const QColor expected(
                        qRound(fill.red() * fill.alphaF() + base.red() * (1 - fill.alphaF())),
                        qRound(fill.green() * fill.alphaF() + base.green() * (1 - fill.alphaF())),
                        qRound(fill.blue() * fill.alphaF() + base.blue() * (1 - fill.alphaF())));
                    QVERIFY(qAbs(pixel.red() - expected.red()) <= 2);
                    QVERIFY(qAbs(pixel.green() - expected.green()) <= 2);
                    QVERIFY(qAbs(pixel.blue() - expected.blue()) <= 2);
                    const auto shadowPoint =
                        neutral->mapToScene(QPointF(neutral->width() / 2, neutral->height() + 1));
                    const auto shadow = image.pixelColor(qRound(shadowPoint.x() * dpr),
                                                         qRound(shadowPoint.y() * dpr));
                    QVERIFY2(shadow.red() < base.red(),
                             "The source's outer action shadow must actually render");
                }
                mouse(neutral);
                QTRY_COMPARE(color(neutral), alpha(tone, 0.10));
                const auto point =
                    neutral->mapToScene(QPointF(neutral->width() / 2, neutral->height() / 2))
                        .toPoint();
                QVERIFY(Test::prepareNativeInput(session.window));
                QTest::mousePress(session.window, Qt::LeftButton, Qt::NoModifier, point);
                QTRY_VERIFY(neutral->property("down").toBool());
                QTRY_COMPARE(color(neutral), alpha(tone, 0.14));
                QTest::mouseRelease(session.window, Qt::LeftButton, Qt::NoModifier, point);
                Test::moveNativePointer(session.window, QPoint(700, 250));
                neutral->forceActiveFocus(Qt::TabFocusReason);
                QTRY_VERIFY(neutral->property("visualFocus").toBool());
                QTRY_COMPARE(color(neutral), alpha(tone, 0.10));
                mouse(accent);
                QTRY_COMPARE(color(accent), alpha(session.theme.accent(), 0.18));
                mouse(select);
                QTRY_COMPARE(color(select), alpha(tone, 0.10));
                const auto selectPoint =
                    select->mapToScene(QPointF(select->width() / 2, select->height() / 2))
                        .toPoint();
                QTest::mouseClick(session.window, Qt::LeftButton, Qt::NoModifier, selectPoint);
                QPointer<QObject> popup = select->property("popup").value<QObject *>();
                QVERIFY(popup);
                QTRY_VERIFY(popup->property("visible").toBool());
                QTRY_COMPARE(color(select), alpha(session.theme.accent(), 0.12));
                QCOMPARE(select->property("foreground").value<QColor>(),
                         floating ? session.theme.floatingAccentText()
                                  : session.theme.accentText());
                QCOMPARE(popup->property("background")
                             .value<QQuickItem *>()
                             ->property("radius")
                             .toReal(),
                         10.0);
                QMetaObject::invokeMethod(popup, "close");
                QTRY_VERIFY(popup.isNull());
                select->forceActiveFocus(Qt::MouseFocusReason);
            }
        QCOMPARE(warnings.count(), 0);
    }
};
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    configureUiTypography();
    ControlsUiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "ControlsUiTest.moc"
