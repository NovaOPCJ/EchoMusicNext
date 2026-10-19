#include <gio/gio.h>
#include "SystemNetworkMonitor.h"
#include <QtTest>
using namespace EchoMusic;
class LinuxNetworkMonitorTest : public QObject
{
    Q_OBJECT
private slots:
    void gsettingsRootAndChildChangesAreDeliveredButAppearanceIsIgnored()
    {
        QObject owner;
        int changes = 0;
        createSystemNetworkMonitor(&owner, [&](bool proxyOnly) {
            QVERIFY(proxyOnly);
            ++changes;
        });
        auto *proxy = g_settings_new("org.gnome.system.proxy");
        auto *http = g_settings_get_child(proxy, "http");
        auto *unrelated = g_settings_new("org.echomusic.network-test");
        const auto cleanup = qScopeGuard([&] {
            g_object_unref(unrelated);
            g_object_unref(http);
            g_object_unref(proxy);
        });
        // The backend starts asynchronously. Keep changing the same in-memory
        // setting until its subscription is live; never write desktop settings.
        for (int i = 0; i < 100 && changes == 0; ++i) {
            QVERIFY(g_settings_set_string(proxy, "mode", i % 2 ? "manual" : "auto"));
            QTest::qWait(10);
        }
        QVERIFY(changes > 0);
        QTest::qWait(50);
        const int beforeChild = changes;
        QVERIFY(g_settings_set_string(http, "host", "notification-test.invalid"));
        QTRY_VERIFY(changes > beforeChild);
        QTest::qWait(50);
        const int beforeUnrelated = changes;
        QVERIFY(g_settings_set_string(unrelated, "appearance", "dark"));
        QTest::qWait(100);
        QCOMPARE(changes, beforeUnrelated);
    }
    void immediateDestructionDrainsWorkerEvenBeforeMainLoopStarts()
    {
        // The CTest process deadline catches a lost stop source/deadlocked
        // join. Thread startup timing under parallel builds is not a product SLA.
        for (int i = 0; i < 20; ++i) {
            QObject owner;
            createSystemNetworkMonitor(&owner, [](bool) {});
        }
    }
};
int main(int argc, char **argv)
{
    qputenv("GSETTINGS_BACKEND", "memory");
    qputenv("GSETTINGS_SCHEMA_DIR", ECHO_NETWORK_TEST_SCHEMA_DIR);
    QCoreApplication app(argc, argv);
    LinuxNetworkMonitorTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "LinuxNetworkMonitorTest.moc"
