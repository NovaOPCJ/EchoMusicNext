#include "LogService.h"
#include "ApiClient.h"
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QSignalSpy>
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDateTime>
using namespace EchoMusic;
class LoggingTest final : public QObject
{
    Q_OBJECT
private slots:
    void preferenceFailureRetryAndExplicitLaunchOverride()
    {
        QTemporaryDir directory;
        const auto blocked = directory.filePath("blocked");
        QFile obstruction(blocked);
        QVERIFY(obstruction.open(QIODevice::WriteOnly));
        obstruction.close();
        QSettings invalid(blocked + "/settings.ini", QSettings::IniFormat);
        {
            LogService logging(directory.filePath("logs"), 2, false, &invalid);
            QSignalSpy notices(&logging, &LogService::notice);
            logging.setLevel("warn");
            QCOMPARE(logging.level(), QString("info"));
            QCOMPARE(notices.size(), 1);
            QVERIFY(QFile::remove(blocked));
            QVERIFY(QDir().mkpath(blocked));
            logging.setLevel("warn");
            QCOMPARE(logging.level(), QString("warn"));
        }
        QSettings saved(blocked + "/settings.ini", QSettings::IniFormat);
        {
            LogService explicitLevel(directory.filePath("override"), 3, false, &saved, true);
            QCOMPARE(explicitLevel.level(), QString("debug"));
        }
        QCOMPARE(saved.value("logging/level").toString(), QString("warn"));
        {
            LogService savedLevel(directory.filePath("restored"), 2, false, &saved);
            QCOMPARE(savedLevel.level(), QString("warn"));
        }
    }
    void liveLevelDiagnosticsAndRedactedApiResponses()
    {
        QTemporaryDir directory;
        QSettings preferences(directory.filePath("settings.ini"), QSettings::IniFormat);
        preferences.setValue("logging/level", "warn");
        QTcpServer proxy;
        QVERIFY(proxy.listen(QHostAddress::LocalHost));
        connect(&proxy, &QTcpServer::newConnection, &proxy, [&] {
            auto *peer = proxy.nextPendingConnection();
            connect(peer, &QTcpSocket::readyRead, peer, [peer] {
                const auto bytes = peer->property("header").toByteArray() + peer->readAll();
                peer->setProperty("header", bytes);
                if (!bytes.contains("\r\n\r\n") || peer->property("answered").toBool())
                    return;
                peer->setProperty("answered", true);
                // Transport diagnostic fixture only, deliberately rejected by
                // HTTP status before any music-service response converter.
                const auto body =
                    QJsonDocument(
                        QJsonObject{{"message", "useful response detail"},
                                    {"token", "response-secret"},
                                    {"nested",
                                     QJsonObject{{"t1", "refresh-secret"},
                                                 {"url", "https://signed.invalid/a?key=secret"}}}})
                        .toJson();
                peer->write("HTTP/1.1 503 Unavailable\r\nContent-Type: "
                            "application/json\r\nContent-Length: " +
                            QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" +
                            body);
                peer->disconnectFromHost();
            });
            connect(peer, &QTcpSocket::disconnected, peer, &QObject::deleteLater);
        });
        {
            LogService logging(directory.path(), 2, false, &preferences);
            QVERIFY(logging.active());
            QCOMPARE(logging.level(), QString("warn"));
            qInfo("hidden-before-live-change");
            logging.setLevel("debug");
            qDebug("visible-after-live-change");
            logging.setLevel("invalid-level");
            QCOMPARE(logging.level(), QString("debug"));
            logging.setApiResponseBody(true);
            logging.enableTemporaryDiagnostics();
            QVERIFY(logging.diagnosticActive());
            QCOMPARE(logging.diagnosticMinutes(), 10);
            logging.setLevel("error");
            // Temporary verbosity must not overwrite the user's base setting.
            QCOMPARE(logging.level(), QString("error"));
            qDebug("visible-during-diagnostics");
            ApiClient api(DeviceIdentity::temporary());
            QVERIFY(api.setNetworkTransport(proxy.serverPort()));
            QSignalSpy failed(&api, &ApiClient::failed);
            QVERIFY(api.audioEffects(ApiClient::EffectCategory::Artist));
            QTRY_COMPARE(failed.size(), 1);
            QTest::qWait(100);
            logging.disableTemporaryDiagnostics();
            QVERIFY(!logging.diagnosticActive());
            qDebug("hidden-after-diagnostics");
        }
        QByteArray bytes;
        for (const auto &name : QDir(directory.path()).entryList({"*.log"}, QDir::Files)) {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            bytes += file.readAll();
        }
        QVERIFY(bytes.contains("visible-after-live-change"));
        QVERIFY(bytes.contains("visible-during-diagnostics"));
        QVERIFY(bytes.contains("useful response detail"));
        QVERIFY(bytes.contains("native.api.response"));
        for (const auto &secret : {"response-secret", "refresh-secret", "signed.invalid",
                                   "hidden-before-live-change", "hidden-after-diagnostics"})
            QVERIFY(!bytes.contains(secret));
        {
            LogService restored(directory.filePath("restored"), 2, false, &preferences);
            QCOMPARE(restored.level(), QString("error"));
            QVERIFY(restored.apiResponseBody());
            QVERIFY(!restored.diagnosticActive());
        }
        preferences.setValue("logging/diagnosticUntil", QDateTime::currentMSecsSinceEpoch() - 1000);
        {
            LogService expired(directory.filePath("expired"), 2, false, &preferences);
            QVERIFY(!expired.diagnosticActive());
            QCOMPARE(expired.level(), QString("error"));
        }
        preferences.setValue("logging/diagnosticUntil", QDateTime::currentMSecsSinceEpoch() + 500);
        {
            LogService timed(directory.filePath("timed"), 2, false, &preferences);
            QVERIFY(timed.diagnosticActive());
            qDebug("visible-before-expiry");
            QTRY_VERIFY_WITH_TIMEOUT(!timed.diagnosticActive(), 3000);
            qDebug("hidden-after-expiry");
            QCOMPARE(timed.level(), QString("error"));
        }
        for (const auto &name :
             QDir(directory.filePath("timed")).entryList({"*.log"}, QDir::Files)) {
            QFile file(directory.filePath("timed/") + name);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto logged = file.readAll();
            QVERIFY(logged.contains("visible-before-expiry"));
            QVERIFY(!logged.contains("hidden-after-expiry"));
        }
    }
    void rustQtAndQmlReachTheSameQtLogFile()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        {
            LogService logging(directory.path(), 3, false);
            QVERIFY(logging.active());
            qInfo("qt-visible-message");
            qWarning("password=must-not-persist");
            QQmlEngine engine;
            QQmlComponent component(&engine);
            component.setData(
                "import QtQml\nQtObject { Component.onCompleted: { "
                "console.info('qml-visible-message'); console.debug('qml-debug-message'); } }",
                QUrl());
            QScopedPointer<QObject> root(component.create());
            QVERIFY2(root, qPrintable(component.errorString()));
            ApiClient api;
            QSignalSpy success(&api, &ApiClient::succeeded);
            QVERIFY(api.capabilities());
            QTRY_COMPARE(success.size(), 1);
            // Exercise the production Rust -> synchronous drain -> Qt handler
            // path. No synthetic native log or upstream response fixture.
            QTest::qWait(100);
        }
        const auto files = QDir(directory.path()).entryList({"*.log"}, QDir::Files);
        QCOMPARE(files.size(), 1);
        QFile file(directory.filePath(files.first()));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto text = file.readAll();
        QVERIFY(text.contains("qt-visible-message"));
        QVERIFY(text.contains("qml-visible-message"));
        QVERIFY(text.contains("qml-debug-message"));
        QVERIFY(text.contains("native.api"));
        QVERIFY(text.contains("echo.api.capabilities"));
        QVERIFY(text.contains("[redacted sensitive log]"));
        QVERIFY(!text.contains("must-not-persist"));
        for (const auto &line : text.split('\n')) {
            if (line.isEmpty())
                continue;
            const auto record = QJsonDocument::fromJson(line).object();
            QVERIFY(record.contains("timestamp"));
            QVERIFY(record.contains("level"));
        }
    }
    void qtDebugIsFilteredAndRotationRetainsWholeRecords()
    {
        QTemporaryDir directory;
        {
            LogService logging(directory.path(), 2, false);
            qDebug("disabled-debug-message");
            // Produce >5 MiB while allowing the bounded worker queue to drain.
            const QString payload(7800, QChar(u'x'));
            for (int batch = 0; batch < 20; ++batch) {
                for (int row = 0; row < 40; ++row)
                    qInfo().noquote() << payload;
                QTest::qWait(25);
            }
            qInfo("last-record-after-rotation");
        }
        const auto files = QDir(directory.path()).entryList({"*.log"}, QDir::Files);
        QVERIFY(files.size() >= 2);
        QVERIFY(files.size() <= 7);
        bool last = false;
        for (const auto &name : files) {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto bytes = file.readAll();
            QVERIFY(!bytes.contains("disabled-debug-message"));
            last = last || bytes.contains("last-record-after-rotation");
            for (const auto &line : bytes.split('\n')) {
                if (!line.isEmpty())
                    QVERIFY(QJsonDocument::fromJson(line).isObject());
            }
        }
        QVERIFY(last);
    }
};
QTEST_GUILESS_MAIN(LoggingTest)
#include "LoggingTest.moc"
