#include "RelaunchService.h"
#include "SingleInstance.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QLockFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>
using namespace EchoMusic;
namespace
{
void mark(const QString &path)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write("completed");
}
} // namespace
class RelaunchTest : public QObject
{
    Q_OBJECT
private slots:
    void committedHelperWaitsForMaintenanceBeforeLockPolling()
    {
        QTemporaryDir directory;
        QLockFile lock(directory.filePath("instance.lock"));
        QVERIFY(lock.tryLock());
        QLocalServer server;
        server.setSocketOptions(QLocalServer::UserAccessOption);
        const auto name = "emnext-r-" + QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY(server.listen(name));
        QProcess helper;
        helper.start(QCoreApplication::applicationFilePath(), {"--relaunch-helper", name});
        QTRY_VERIFY(server.hasPendingConnections());
        auto *socket = server.nextPendingConnection();
        socket->write(QJsonDocument(QJsonObject{{"lock", directory.filePath("instance.lock")},
                                                {"arguments", QJsonArray{"--restart-fixture-child",
                                                                         directory.path()}}})
                          .toJson(QJsonDocument::Compact) +
                      '\n');
        QTRY_VERIFY(socket->canReadLine());
        QCOMPARE(socket->readLine(), QByteArray("ready-v1\n"));
        socket->write("commit-v1\n");
        QTRY_VERIFY(socket->canReadLine());
        QCOMPARE(socket->readLine(), QByteArray("committed-v1\n"));
        // Exceed the preparation deadline while maintenance owns the original
        // process. A committed helper must not silently expire during restore.
        QTest::qWait(10500);
        QCOMPARE(helper.state(), QProcess::Running);
        lock.unlock();
        QTest::qWait(100);
        QVERIFY(!QFile::exists(directory.filePath("successor")));
        socket->disconnectFromServer();
        QTRY_COMPARE(helper.state(), QProcess::NotRunning);
        QCOMPARE(helper.exitCode(), 0);
        QTRY_VERIFY(QFile::exists(directory.filePath("successor")));
    }
    void productionCoreHelperRelaunchesIsolatedSmokeAfterLockRelease()
    {
        const auto executable = qEnvironmentVariable("ECHO_RELAUNCH_APPLICATION");
        if (executable.isEmpty())
            QSKIP("Opt-in production executable helper/smoke check");
        QTemporaryDir directory;
        QLockFile lock(directory.filePath("instance.lock"));
        lock.setStaleLockTime(0);
        QVERIFY(lock.tryLock(0));
        QLocalServer server;
        server.setSocketOptions(QLocalServer::UserAccessOption);
        const auto name = "emnext-r-" + QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY(server.listen(name));
        QProcess helper;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM", "offscreen");
        helper.setProcessEnvironment(environment);
        helper.start(executable, {"--relaunch-helper", name});
        QTRY_VERIFY2(server.hasPendingConnections(), helper.readAllStandardError().constData());
        auto *socket = server.nextPendingConnection();
        const auto logs = directory.filePath("logs");
        socket->write(
            QJsonDocument(QJsonObject{{"lock", directory.filePath("instance.lock")},
                                      {"arguments", QJsonArray{"--smoke-test", "--log-dir", logs}}})
                .toJson(QJsonDocument::Compact) +
            '\n');
        QTRY_VERIFY(socket->canReadLine());
        QCOMPARE(socket->readLine(), QByteArray("ready-v1\n"));
        socket->write("commit-v1\n");
        QTRY_VERIFY(socket->canReadLine());
        QCOMPARE(socket->readLine(), QByteArray("committed-v1\n"));
        socket->disconnectFromServer();
        QTest::qWait(100);
        QCOMPARE(helper.state(), QProcess::Running);
        QVERIFY(!QDir(logs).exists());
        lock.unlock();
        QTRY_COMPARE(helper.state(), QProcess::NotRunning);
        QCOMPARE(helper.exitCode(), 0);
        const auto drained = [&] {
            for (const auto &path : QDir(logs).entryList({"echomusicnext-*.log"}, QDir::Files)) {
                QFile file(QDir(logs).filePath(path));
                if (file.open(QIODevice::ReadOnly)) {
                    const auto bytes = file.readAll();
                    if (bytes.contains("audio player runtime shut down") &&
                        bytes.contains("logging stopped; draining accepted entries") &&
                        !bytes.contains("Smoke timed out"))
                        return true;
                }
            }
            return false;
        };
        const bool completed = QTest::qWaitFor(drained, 15000);
        QByteArray diagnostics = helper.readAllStandardError();
        for (const auto &path : QDir(logs).entryList({"echomusicnext-*.log"}, QDir::Files)) {
            QFile file(QDir(logs).filePath(path));
            if (file.open(QIODevice::ReadOnly))
                diagnostics += file.readAll();
        }
        QVERIFY2(completed, diagnostics.constData());
    }
    void successorWaitsForNormalTeardownAndInstanceLock()
    {
        QTemporaryDir directory;
        QProcess parent;
        parent.start(QCoreApplication::applicationFilePath(),
                     {"--restart-fixture-parent", directory.path()});
        const auto prepared = directory.filePath("prepared");
        const auto successor = directory.filePath("successor");
        QTRY_VERIFY2(QFile::exists(prepared), parent.readAllStandardError().constData());
        QCOMPARE(parent.state(), QProcess::Running);
        QLockFile probe(directory.filePath("instance.lock"));
        probe.setStaleLockTime(0);
        QVERIFY(!probe.tryLock(0));
        QVERIFY(!QFile::exists(successor));
        QTRY_COMPARE(parent.state(), QProcess::NotRunning);
        QCOMPARE(parent.exitCode(), 0);
        QTRY_VERIFY(QFile::exists(successor));
        QVERIFY(!QFile::exists(directory.filePath("premature-successor")));
    }
    void helperCannotRelaunchBeforeCommit()
    {
        QTemporaryDir directory;
        QLocalServer server;
        server.setSocketOptions(QLocalServer::UserAccessOption);
        const auto name = "emnext-r-" + QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY2(server.listen(name), qPrintable(server.errorString()));
        QProcess helper;
        helper.start(QCoreApplication::applicationFilePath(), {"--relaunch-helper", name});
        QTRY_VERIFY(server.hasPendingConnections());
        auto *socket = server.nextPendingConnection();
        const auto message =
            QJsonDocument(
                QJsonObject{{"lock", directory.filePath("instance.lock")},
                            {"arguments", QJsonArray{"--restart-fixture-child", directory.path()}}})
                .toJson(QJsonDocument::Compact);
        socket->write(message + '\n');
        QTRY_VERIFY(socket->canReadLine());
        QCOMPARE(socket->readLine(), QByteArray("ready-v1\n"));
        socket->disconnectFromServer();
        QTRY_COMPARE(helper.state(), QProcess::NotRunning);
        QCOMPARE(helper.exitCode(), 1);
        QVERIFY(!QFile::exists(directory.filePath("successor")));
    }
    void rejectedPreparationDoesNotQuitAndAllowsRetry()
    {
        RelaunchService relaunch("relative/instance.lock", {}, true);
        QSignalSpy quit(&relaunch, &RelaunchService::readyToQuit);
        QSignalSpy notices(&relaunch, &RelaunchService::notice);
        relaunch.restart();
        QTRY_COMPARE(notices.size(), 1);
        QVERIFY(!relaunch.busy());
        QCOMPARE(quit.size(), 0);
        relaunch.restart();
        QTRY_COMPARE(notices.size(), 2);
        QVERIFY(!relaunch.busy());
        QCOMPARE(quit.size(), 0);
    }
    void developmentModeKeepsProcessAndOneShotArgumentsAreRemoved()
    {
        RelaunchService relaunch({}, {}, false);
        QSignalSpy quit(&relaunch, &RelaunchService::readyToQuit);
        QSignalSpy notices(&relaunch, &RelaunchService::notice);
        relaunch.restart();
        QCOMPARE(notices.size(), 1);
        QCOMPARE(quit.size(), 0);
        QVERIFY(!relaunch.busy());
        QCOMPARE(RelaunchService::persistentArguments(
                     {"app", "--plugin-dir", "/one-shot", "--safe-mode", "--log-dir=/local logs",
                      "--open-url", "echomusicnext://plugin?one-shot", "--log-level", "debug",
                      "--smoke-test"}),
                 QStringList({"--safe-mode", "--log-dir=/local logs", "--log-level", "debug"}));
    }
};
int main(int argc, char **argv)
{
    if (RelaunchService::isHelperInvocation(argc, argv))
        return RelaunchService::runHelper(argc, argv);
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.size() == 3 && arguments[1] == "--restart-fixture-parent") {
        SingleInstance instance;
        if (instance.start(arguments[2]) != SingleInstance::Result::Primary)
            return 3;
        RelaunchService relaunch(instance.lockFilePath(), {"--restart-fixture-child", arguments[2]},
                                 true);
        QObject::connect(&relaunch, &RelaunchService::readyToQuit, &app, [&] {
            mark(arguments[2] + "/prepared");
            // Deliberately keep the old instance live after commit. A helper
            // that launches immediately would only activate this dying process.
            QTimer::singleShot(800, &app, &QCoreApplication::quit);
        });
        QObject::connect(&relaunch, &RelaunchService::notice, &app, [&] { app.exit(4); });
        QTimer::singleShot(0, &relaunch, &RelaunchService::restart);
        QTimer::singleShot(5000, &app, [&] { app.exit(5); });
        return app.exec();
    }
    if (arguments.size() == 3 && arguments[1] == "--restart-fixture-child") {
        SingleInstance instance;
        if (instance.start(arguments[2]) != SingleInstance::Result::Primary) {
            mark(arguments[2] + "/premature-successor");
            return 6;
        }
        mark(arguments[2] + "/successor");
        return 0;
    }
    RelaunchTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "RelaunchTest.moc"
