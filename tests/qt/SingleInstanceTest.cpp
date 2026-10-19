#include "SingleInstance.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
#include <cstdio>
#include <memory>
#include <vector>
using namespace EchoMusic;
namespace
{
void report(const char *message)
{
    std::puts(message);
    std::fflush(stdout);
}
struct Child
{
    QProcess process;
    QByteArray output;
    void start(const QString &directory, bool quitNormally = false, const QUrl &url = {})
    {
        auto args = QStringList{"--instance-helper", directory};
        if (!url.isEmpty())
            args.append(url.toString());
        if (quitNormally)
            args.append("--quit-normally");
        process.start(QCoreApplication::applicationFilePath(), args);
    }
    bool contains(const QByteArray &text)
    {
        output += process.readAllStandardOutput();
        return output.contains(text);
    }
    ~Child()
    {
        if (process.state() != QProcess::NotRunning) {
            process.kill();
            process.waitForFinished(3000);
        }
    }
};
} // namespace
class SingleInstanceTest : public QObject
{
    Q_OBJECT
private slots:
    void secondaryAcknowledgesActivationWithoutStartingBusiness()
    {
        QTemporaryDir directory;
        Child primary;
        primary.start(directory.path());
        QTRY_VERIFY(primary.contains("PRIMARY"));
        Child secondary;
        secondary.start(directory.path());
        QTRY_VERIFY(secondary.contains("SECONDARY"));
        QTRY_COMPARE(secondary.process.state(), QProcess::NotRunning);
        QCOMPARE(secondary.process.exitCode(), 0);
        QTRY_VERIFY(primary.contains("ACTIVATED"));
        QCOMPARE(primary.process.state(), QProcess::Running);
    }
    void secondaryForwardsResourceUrl_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::newRow("share") << QUrl("echomusicnext://plugin?source=https://github.com/owner/"
                                       "repo&id=org.test.plugin&version=1.0.0&path=plugins/test");
        QTest::newRow("song") << QUrl(
            "echomusicnext://song/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa?albumId=42&albumAudioId=99");
        QTest::newRow("album") << QUrl("echomusicnext://album/42");
        QTest::newRow("room") << QUrl(
            "echomusicnext://listen-together/123?roomType=0&roomName=%E6%99%9A%E9%A3%8E");
        QTest::newRow("echomod-file")
            << QUrl::fromLocalFile(QDir::tempPath() + "/audio core.echomod");
    }
    void secondaryForwardsResourceUrl()
    {
        QFETCH(QUrl, url);
        QTemporaryDir directory;
        Child primary;
        primary.start(directory.path());
        QTRY_VERIFY(primary.contains("PRIMARY"));
        Child secondary;
        secondary.start(directory.path(), false, url);
        QTRY_VERIFY(secondary.contains("SECONDARY"));
        QTRY_VERIFY(primary.contains(url.toEncoded()));
        QCOMPARE(primary.process.state(), QProcess::Running);
    }
    void simultaneousLaunchesElectExactlyOneOwner()
    {
        QTemporaryDir directory;
        std::vector<std::unique_ptr<Child>> children;
        for (int i = 0; i < 6; ++i) {
            auto child = std::make_unique<Child>();
            child->start(directory.path());
            children.push_back(std::move(child));
        }
        int owners = 0, forwarded = 0;
        for (const auto &child : children) {
            QTRY_VERIFY(child->contains("PRIMARY") || child->contains("SECONDARY"));
            if (child->contains("PRIMARY"))
                ++owners;
            else {
                ++forwarded;
                QTRY_COMPARE(child->process.state(), QProcess::NotRunning);
                QCOMPARE(child->process.exitCode(), 0);
            }
        }
        QCOMPARE(owners, 1);
        QCOMPARE(forwarded, 5);
    }
    void crashRecoversLockAndSocket()
    {
        QTemporaryDir directory;
        Child primary;
        primary.start(directory.path());
        QTRY_VERIFY(primary.contains("PRIMARY"));
        primary.process.kill();
        QVERIFY(primary.process.waitForFinished(3000));
        SingleInstance successor;
        QCOMPARE(successor.start(directory.path()), SingleInstance::Result::Primary);
    }
    void cleanExitReleasesOwnership()
    {
        QTemporaryDir directory;
        Child primary;
        primary.start(directory.path(), true);
        QTRY_VERIFY(primary.contains("PRIMARY"));
        QTRY_COMPARE(primary.process.state(), QProcess::NotRunning);
        QCOMPARE(primary.process.exitCode(), 0);
        QVERIFY(!QFile::exists(directory.filePath("instance.lock")));
        SingleInstance successor;
        QCOMPARE(successor.start(directory.path()), SingleInstance::Result::Primary);
    }
    void unresponsiveOwnerIsNotReplaced()
    {
        QTemporaryDir directory;
        QLockFile liveOwner(directory.filePath("instance.lock"));
        liveOwner.setStaleLockTime(0);
        QVERIFY(liveOwner.tryLock(0));
        SingleInstance second;
        QCOMPARE(second.start(directory.path(), 100), SingleInstance::Result::Failed);
        QVERIFY(liveOwner.isLocked());
        QVERIFY(!second.errorString().isEmpty());
    }
    void invalidDirectoryCannotStart()
    {
        QTemporaryDir directory;
        QFile file(directory.filePath("file"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        SingleInstance instance;
        QCOMPARE(instance.start(file.fileName()), SingleInstance::Result::Failed);
    }
    void protocolHandlesFragmentsAndRejectsUnknownCommands()
    {
        QTemporaryDir directory;
        Child primary;
        primary.start(directory.path());
        QTRY_VERIFY(primary.contains("PRIMARY"));
        const auto digest = QCryptographicHash::hash(
            QFileInfo(directory.path()).canonicalFilePath().toUtf8(), QCryptographicHash::Sha256);
        const auto name = "echomusicnext-" + QString::fromLatin1(digest.toHex().left(24));
        QLocalSocket invalid;
        invalid.connectToServer(name);
        QVERIFY(invalid.waitForConnected(1000));
        invalid.write(QByteArray(64, 'x'));
        invalid.flush();
        QTRY_COMPARE(invalid.state(), QLocalSocket::UnconnectedState);
        QVERIFY(!primary.contains("ACTIVATED"));
        QLocalSocket fragmented;
        fragmented.connectToServer(name);
        QVERIFY(fragmented.waitForConnected(1000));
        fragmented.write("activate-");
        fragmented.flush();
        QTest::qWait(50);
        QVERIFY(!primary.contains("ACTIVATED"));
        fragmented.write("v1\n");
        fragmented.flush();
        QVERIFY(fragmented.waitForReadyRead(1000));
        QCOMPARE(fragmented.readAll(), QByteArray("accepted-v1\n"));
        QTRY_VERIFY(primary.contains("ACTIVATED"));
    }
};
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.contains("--instance-helper")) {
        SingleInstance instance;
        QObject::connect(&instance, &SingleInstance::activationRequested,
                         [] { report("ACTIVATED"); });
        QObject::connect(&instance, &SingleInstance::urlRequested,
                         [](const QUrl &url) { report(url.toEncoded().constData()); });
        const QUrl candidate(args.value(3));
        const auto url =
            candidate.scheme() == "echomusicnext" || candidate.isLocalFile() ? candidate : QUrl();
        const auto result = instance.start(args.value(2), 5000, url);
        if (result == SingleInstance::Result::Activated) {
            report("SECONDARY");
            return 0;
        }
        if (result != SingleInstance::Result::Primary)
            return 1;
        report("PRIMARY");
        QTimer::singleShot(args.contains("--quit-normally") ? 100 : 30000, &app,
                           &QCoreApplication::quit);
        return app.exec();
    }
    SingleInstanceTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "SingleInstanceTest.moc"
