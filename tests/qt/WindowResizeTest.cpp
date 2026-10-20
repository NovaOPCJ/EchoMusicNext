#include "WindowResizeSync.h"
#include "NativeWindow.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QResizeEvent>
#include <QSGRendererInterface>
#include <QAbstractEventDispatcher>
#include <QScopeGuard>
#include <QtTest>
#include <atomic>
#include <memory>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace EchoMusic;
class ResizeWindow final : public QQuickWindow
{
public:
    std::shared_ptr<std::atomic<int>> frames = std::make_shared<std::atomic<int>>(0);
    int resizes = 0;
    int frameAtResize = 0;
    void resizeEvent(QResizeEvent *event) override
    {
        ++resizes;
        frameAtResize = frames->load();
        QQuickWindow::resizeEvent(event);
    }
};
class WindowResizeTest : public QObject
{
    Q_OBJECT
private slots:
    void nativeObserverSurvivesBypassedSubclass()
    {
#ifndef Q_OS_WIN
        QSKIP("The independent Win32 message path requires Windows");
#else
        ResizeWindow window;
        window.resize(500, 400);
        connect(
            &window, &QQuickWindow::afterFrameEnd, &window,
            [frames = window.frames] { frames->fetch_add(1); }, Qt::DirectConnection);
        window.show();
        QVERIFY(Test::prepareNativeInput(&window));
        const auto api = window.rendererInterface()->graphicsApi();
        if (!QSGRendererInterface::isApiRhiBased(api) || api == QSGRendererInterface::Null)
            QSKIP("The presentation barrier requires a native RHI window");
        QTRY_VERIFY(window.frames->load() > 0);
        const auto handle = reinterpret_cast<HWND>(window.winId());
        const auto originalProcedure = GetWindowLongPtrW(handle, GWLP_WNDPROC);
        QVERIFY(originalProcedure);
        int waits = 0;
        WindowResizeSync sync(&window, [&] {
            ++waits;
            QCOMPARE(window.contentItem()->size(), QSizeF(window.size()));
            QVERIFY(window.frames->load() > window.frameAtResize);
        });
        {
            // Simulate a later WNDPROC replacement bypassing comctl32's chain.
            // Production code never replaces Qt's procedure. Restore before
            // the controller or native surface is destroyed, even on failure.
            const auto subclassProcedure =
                SetWindowLongPtrW(handle, GWLP_WNDPROC, originalProcedure);
            QVERIFY(subclassProcedure && subclassProcedure != originalProcedure);
            const auto restore =
                qScopeGuard([&] { SetWindowLongPtrW(handle, GWLP_WNDPROC, subclassProcedure); });
            MSG foreign{};
            foreign.hwnd = GetDesktopWindow();
            foreign.message = WM_ENTERSIZEMOVE;
            qintptr result = 0;
            QVERIFY(!QCoreApplication::eventDispatcher()->filterNativeEvent("windows_generic_MSG",
                                                                            &foreign, &result));
            window.resize(520, 400);
            QTest::qWait(100);
            QCOMPARE(waits, 0); // A different HWND must not activate this window.
            SendMessageW(handle, WM_ENTERSIZEMOVE, 0, 0);
            window.resize(540, 400);
            QTRY_COMPARE(waits, 1);
            SendMessageW(handle, WM_EXITSIZEMOVE, 0, 0);
            window.resize(560, 400);
            QTest::qWait(100);
            QCOMPARE(waits, 1);
        }
        window.destroy();
        window.show();
        QVERIFY(Test::prepareNativeInput(&window));
        const auto recreated = reinterpret_cast<HWND>(window.winId());
        SendMessageW(recreated, WM_ENTERSIZEMOVE, 0, 0);
        window.resize(600, 400);
        QTRY_COMPARE(waits, 2);
        SendMessageW(recreated, WM_EXITSIZEMOVE, 0, 0);
#endif
    }
    void newSizeFrameCompletesBeforeCompositorWait()
    {
        ResizeWindow window;
        auto format = window.format();
        format.setAlphaBufferSize(8);
        window.setFormat(format);
        window.setFlags(Qt::Window | Qt::FramelessWindowHint);
        window.setColor(Qt::transparent);
        window.resize(500, 400);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
import QtQuick
import QtQuick.Effects
Item {
 anchors.fill: parent
 Rectangle { id: image; anchors.fill: parent; visible: false; color: "#805078a0" }
 MultiEffect { anchors.fill: parent; source: image; brightness: 0.1 }
 Text { anchors.centerIn: parent; text: "Resize GPU / 缩放"; color: "white" }
})",
                          QUrl());
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(root.data());
        QVERIFY(item);
        item->setParentItem(window.contentItem());
        connect(
            &window, &QQuickWindow::afterFrameEnd, &window,
            [frames = window.frames] { frames->fetch_add(1); }, Qt::DirectConnection);
        window.show();
        QVERIFY(Test::prepareNativeInput(&window));
        QVERIFY(window.requestedFormat().alphaBufferSize() > 0);
        const auto api = window.rendererInterface()->graphicsApi();
        if (!QSGRendererInterface::isApiRhiBased(api) || api == QSGRendererInterface::Null)
            QSKIP("The presentation barrier requires a native RHI window");
        QTRY_VERIFY(window.frames->load() > 0);
        int waits = 0;
        auto *sync = new WindowResizeSync(&window, [&] {
            ++waits;
            QCOMPARE(window.contentItem()->size(), QSizeF(window.size()));
            QCOMPARE(item->size(), QSizeF(window.size()));
            QVERIFY(window.frames->load() > window.frameAtResize);
        });
        QTest::qWait(150);
#ifdef Q_OS_WIN
        SendMessageW(reinterpret_cast<HWND>(window.winId()), WM_ENTERSIZEMOVE, 0, 0);
#else
        sync->setInteractiveResize(true);
#endif
        const auto beforeMove = waits;
        window.setPosition(window.position() + QPoint(10, 10));
        QTest::qWait(50);
        QCOMPARE(waits, beforeMove); // Moving is not a resize/presentation command.
        for (const QSize size :
             {QSize(520, 410), QSize(510, 430), QSize(540, 450), QSize(500, 400)}) {
            const auto before = waits;
            const auto resizes = window.resizes;
            window.resize(size);
            QTRY_COMPARE(waits, before + 1);
            QCOMPARE(window.resizes, resizes + 1);
            QCOMPARE(window.size(), size);
        }
        QTest::qWait(150);
        const auto stationary = waits;
        QTest::qWait(200);
        QCOMPARE(waits, stationary); // No forced presentations while the pointer is stationary.
        const auto image = window.grabWindow();
        QVERIFY(!image.isNull());
        const auto corner = image.pixelColor(qRound(20 * image.devicePixelRatio()),
                                             qRound(20 * image.devicePixelRatio()));
        QVERIFY(corner.red() > 80 && corner.blue() > 160);     // The GPU effect remains drawn.
        QVERIFY(corner.alpha() > 100 && corner.alpha() < 160); // Alpha composition is retained.
#ifdef Q_OS_WIN
        SendMessageW(reinterpret_cast<HWND>(window.winId()), WM_EXITSIZEMOVE, 0, 0);
#else
        sync->setInteractiveResize(false);
#endif
        const auto completed = waits;
        window.resize(530, 420);
        QTest::qWait(100);
        QCOMPARE(waits, completed); // Ordinary programmatic resize stays asynchronous.
        sync->setInteractiveResize(true);
        window.hide();
        window.resize(540, 430);
        QTest::qWait(50);
        QCOMPARE(waits, completed);
        window.show();
        QVERIFY(Test::prepareNativeInput(&window));
        window.resize(550, 440);
        QTest::qWait(100);
        QCOMPARE(waits, completed); // Hide retired the native resize cycle.
        qInfo() << "Resize graphics API" << api << "synchronous size frames" << completed;
    }
};
QTEST_MAIN(WindowResizeTest)
#include "WindowResizeTest.moc"
