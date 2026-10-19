#include "ShortcutService.h"
#include "ShortcutActions.h"
#include "ShellController.h"
#include "ShellSettings.h"
#include "WindowController.h"
#include <QQuickItem>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
using namespace EchoMusic;
class ShortcutServiceTest : public QObject
{
    Q_OBJECT
private slots:
    void presentationUsesSourceModifierOrderWithoutChangingBindings()
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("shortcuts.ini"), QSettings::IniFormat);
        ShortcutService service(settings, false);
        const auto row = service.index(ShortcutService::SeekBackward);
#if defined(Q_OS_MACOS)
        QCOMPARE(row.data(ShortcutService::LocalLabel).toString(), QString("⌘⌥←"));
        QCOMPARE(row.data(ShortcutService::GlobalLabel).toString(), QString("⌘⇧⌥←"));
#else
        QCOMPARE(row.data(ShortcutService::LocalLabel).toString(), QString("Ctrl+Alt+Left"));
        QCOMPARE(row.data(ShortcutService::GlobalLabel).toString(), QString("Ctrl+Shift+Alt+Left"));
#endif
        service.beginRecording(ShortcutService::SeekBackward, false);
        QVERIFY(service.recordKey(Qt::Key_Return, Qt::ControlModifier | Qt::ShiftModifier));
        QCOMPARE(QKeySequence(settings.value("shortcuts/local/seekBackward").toString(),
                              QKeySequence::PortableText),
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Return));
    }
    void typedActionsUsePlaybackPercentAndExistingMuteState()
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
        ApiClient api(DeviceIdentity::temporary());
        PlaybackService playback(api, settings, nullptr, directory.filePath("playback.sqlite"));
        auto *firstView = new QObject;
        QObject secondView;
        playback.cast()->setBrowsingFor(firstView, true);
        playback.cast()->setBrowsingFor(&secondView, true);
        QVERIFY(playback.cast()->browsing());
        delete firstView;
        QVERIFY(playback.cast()->browsing());
        playback.cast()->setBrowsingFor(&secondView, false);
        QVERIFY(!playback.cast()->browsing());
        RouteController router;
        ShellController shell(settings, router);
        ShellSettings preferences(settings);
        WindowController window(settings, false);
        ShortcutService shortcuts(settings, false);
        ShortcutActions actions(shortcuts, playback, preferences, window, shell);
        playback.setVolume(50);
        shortcuts.triggered(ShortcutService::VolumeUp);
        QCOMPARE(playback.volume(), 55.);
        shortcuts.triggered(ShortcutService::VolumeDown);
        QCOMPARE(playback.volume(), 50.);
        shortcuts.triggered(ShortcutService::ToggleMute);
        QCOMPARE(playback.volume(), 0.);
        shortcuts.triggered(ShortcutService::ToggleMute);
        QCOMPARE(playback.volume(), 50.);
        const auto collapsed = preferences.sidebarCollapsed();
        shortcuts.triggered(ShortcutService::ToggleSidebar);
        QCOMPARE(preferences.sidebarCollapsed(), !collapsed);
        QSignalSpy closed(&playback, &PlaybackService::closed);
        playback.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 15000);
        api.shutdown();
    }
    void nativeRegistrationConflictAndRelease()
    {
#if !defined(Q_OS_MACOS)
        QSKIP("Native macOS registration check");
#else
        if (!qEnvironmentVariableIsSet("ECHO_NATIVE_SHORTCUT_TEST"))
            QSKIP("Run explicitly with the Cocoa platform");
        GlobalShortcuts first, second;
        const QList<GlobalShortcuts::Binding> bindings{
            {77, QKeySequence("Ctrl+Alt+Shift+F19"), "registration lifecycle test"}};
        QSignalSpy failedFirst(&first, &GlobalShortcuts::registrationFailed);
        QSignalSpy failedSecond(&second, &GlobalShortcuts::registrationFailed);
        first.replace(bindings);
        if (!failedFirst.isEmpty())
            QSKIP("The test key is already reserved by this desktop");
        second.replace(bindings);
        QCOMPARE(failedSecond.size(), 1);
        first.replace({});
        failedSecond.clear();
        second.replace(bindings);
        QVERIFY(failedSecond.isEmpty());
        second.replace({});
#endif
    }
    void recordingConflictClearResetAndPersistence()
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
        {
            ShortcutService service(settings, false);
            QCOMPARE(service.rowCount(), 15);
            QVERIFY(!service.globalEnabled());
            service.beginRecording(1, true);
            QCOMPARE(service.recordingRow(), -1);
            service.beginRecording(1, false);
            QVERIFY(!service.recordKey(Qt::Key_Control, Qt::ControlModifier));
            QVERIFY(!service.recordKey(Qt::Key_A, Qt::NoModifier));
            // Default playback binding is already owned by the first command.
            QSignalSpy notice(&service, &ShortcutService::notice);
            QVERIFY(!service.recordKey(Qt::Key_Space, Qt::ControlModifier));
            QCOMPARE(service.recordingRow(), 1);
            QCOMPARE(notice.size(), 1);
            QVERIFY(service.recordKey(Qt::Key_F6, Qt::NoModifier));
            QCOMPARE(service.recordingRow(), -1);
            QCOMPARE(service.data(service.index(1), ShortcutService::LocalLabel).toString(),
                     QString("F6"));
            service.setGlobalEnabled(true);
            service.beginRecording(2, true);
            // The same sequence may intentionally be used in another scope.
            QVERIFY(service.recordKey(Qt::Key_F6, Qt::NoModifier));
            service.beginRecording(3, false);
            QVERIFY(service.recordKey(Qt::Key_Escape, Qt::NoModifier));
            QVERIFY(
                service.data(service.index(3), ShortcutService::LocalLabel).toString().isEmpty());
        }
        ShortcutService restored(settings, false);
        QCOMPARE(restored.data(restored.index(1), ShortcutService::LocalLabel).toString(),
                 QString("F6"));
        QCOMPARE(restored.data(restored.index(2), ShortcutService::GlobalLabel).toString(),
                 QString("F6"));
        QVERIFY(restored.data(restored.index(1), ShortcutService::Modified).toBool());
        restored.resetBinding(1);
        QVERIFY(!restored.data(restored.index(1), ShortcutService::Modified).toBool());
        restored.resetAll();
        for (int row = 0; row < restored.rowCount(); ++row)
            QVERIFY(!restored.data(restored.index(row), ShortcutService::Modified).toBool());
    }
    void windowFocusEditingAndShutdown()
    {
        QTemporaryDir directory;
        QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
        ShortcutService service(settings, false);
        QQuickWindow window;
        service.attach(&window);
        window.resize(320, 240);
        window.show();
        window.requestActivate();
        QTRY_VERIFY(window.isActive());
        QSignalSpy actions(&service, &ShortcutService::triggered);
        QTest::keyClick(&window, Qt::Key_Space, Qt::ControlModifier);
        QCOMPARE(actions.size(), 1);
        QCOMPARE(qvariant_cast<ShortcutService::Command>(actions.first().first()),
                 ShortcutService::TogglePlayback);
        auto editor = new QQuickItem(window.contentItem());
        editor->setFlag(QQuickItem::ItemAcceptsInputMethod);
        editor->forceActiveFocus();
        QTest::keyClick(&window, Qt::Key_Space, Qt::ControlModifier);
        QCOMPARE(actions.size(), 1);
        service.beginRecording(0, false);
        QTest::keyClick(&window, Qt::Key_F9);
        QCOMPARE(service.recordingRow(), -1);
        QCOMPARE(actions.size(), 1);
        editor->setFlag(QQuickItem::ItemAcceptsInputMethod, false);
        QTest::keyClick(&window, Qt::Key_F9);
        QCOMPARE(actions.size(), 2);
        service.shutdown();
        QTest::keyClick(&window, Qt::Key_F9);
        QCOMPARE(actions.size(), 2);
    }
};
QTEST_MAIN(ShortcutServiceTest)
#include "ShortcutServiceTest.moc"
