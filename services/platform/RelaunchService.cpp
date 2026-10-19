#include "RelaunchService.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QLockFile>
#include <QProcess>
#include <QUuid>
#include <cstring>
#include <memory>

namespace EchoMusic
{
namespace
{
bool startDetached(const QStringList &arguments)
{
    QProcess process;
    process.setProgram(QCoreApplication::applicationFilePath());
    process.setArguments(arguments);
    // A detached successor must not retain the old launcher's pipes. Once that
    // launcher closes its reader, console logging can terminate the successor
    // with SIGPIPE. Its own LogService still writes the normal PID-named file.
    process.setStandardInputFile(QProcess::nullDevice());
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    return process.startDetached();
}
} // namespace
RelaunchService::RelaunchService(QString lockPath, QStringList arguments, bool automaticRestart,
                                 QObject *parent)
    : QObject(parent), m_lockPath(std::move(lockPath)), m_arguments(std::move(arguments)),
      m_automatic(automaticRestart && !m_lockPath.isEmpty())
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.setMaxPendingConnections(1);
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, &RelaunchService::fail);
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (m_server.hasPendingConnections()) {
            auto *socket = m_server.nextPendingConnection();
            if (m_socket) {
                socket->abort();
                socket->deleteLater();
                continue;
            }
            m_socket = socket;
            connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
                socket->deleteLater();
                if (!m_committed)
                    fail();
            });
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                m_response += socket->readAll();
                if (m_response.size() > 128) {
                    fail();
                    return;
                }
                if (!m_response.contains('\n'))
                    return;
                const QByteArray expected = m_prepared ? "committed-v1\n" : "ready-v1\n";
                if (m_response != expected) {
                    fail();
                    return;
                }
                m_response.clear();
                if (!m_prepared) {
                    m_prepared = true;
                    socket->write("commit-v1\n");
                    return;
                }
                m_committed = true;
                m_deadline.stop();
                m_server.close();
                // Keep the authenticated helper connection through maintenance
                // and normal teardown. Its lock-wait deadline starts only when
                // this service is destroyed, not while a large restore runs.
                emit readyToQuit();
            });
            socket->write(
                QJsonDocument(QJsonObject{{"lock", m_lockPath},
                                          {"arguments", QJsonArray::fromStringList(m_arguments)}})
                    .toJson(QJsonDocument::Compact) +
                '\n');
        }
    });
}
RelaunchService::~RelaunchService()
{
    m_deadline.stop();
    disconnect(&m_server, nullptr, this, nullptr);
    if (m_socket) {
        disconnect(m_socket, nullptr, this, nullptr);
        m_socket->abort();
    }
    m_server.close();
}
void RelaunchService::restart()
{
    prepareRestart(m_automatic);
}
void RelaunchService::restartForStartupRecovery()
{
    prepareRestart(true); // Explicit retry, including a development application build.
}
void RelaunchService::prepareRestart(bool allowed)
{
    if (m_busy)
        return;
    if (!allowed) {
        emit notice(tr("当前启动方式不支持自动重启，请手动重新启动"));
        return;
    }
    m_response.clear();
    m_prepared = m_committed = false;
    m_busy = true;
    emit changed();
    // macOS AF_UNIX paths include the long per-user temporary directory and
    // allow only 104 bytes. Keep a short prefix and the full random 128-bit ID.
    const auto name = "emnext-r-" + QUuid::createUuid().toString(QUuid::Id128);
    if (!m_server.listen(name) || !startDetached({"--relaunch-helper", name})) {
        fail();
        return;
    }
    m_deadline.start(5000);
}
void RelaunchService::fail()
{
    if (!m_busy || m_committed)
        return;
    m_deadline.stop();
    m_server.close();
    if (m_socket) {
        auto *socket = m_socket.data();
        m_socket.clear();
        disconnect(socket, nullptr, this, nullptr);
        socket->abort();
        socket->deleteLater();
    }
    m_busy = false;
    emit changed();
    emit notice(tr("无法准备应用重启，请稍后重试或手动重新启动"));
}
bool RelaunchService::isHelperInvocation(int argc, char **argv)
{
    return argc == 3 && std::strcmp(argv[1], "--relaunch-helper") == 0;
}
QStringList RelaunchService::persistentArguments(const QStringList &arguments)
{
    QStringList result;
    for (qsizetype i = 1; i < arguments.size(); ++i) {
        const auto &argument = arguments[i];
        if (argument == "--safe-mode") {
            result.append(argument);
            continue;
        }
        const auto equals = argument.indexOf('=');
        const auto key = equals < 0 ? argument : argument.left(equals);
        if (key != "--log-level" && key != "--log-dir")
            continue;
        if (equals >= 0) {
            result.append(argument);
        } else if (i + 1 < arguments.size()) {
            result.append(argument);
            result.append(arguments[++i]);
        }
    }
    return result;
}
int RelaunchService::runHelper(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto name = app.arguments().value(2);
    if (!name.startsWith("emnext-r-") || name.size() > 128)
        return 2;
    QLocalSocket socket;
    QTimer deadline, lockPoll;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] { app.exit(1); });
    deadline.start(10000);
    QByteArray input;
    QStringList arguments;
    std::unique_ptr<QLockFile> lock;
    bool configured = false, committed = false;
    QObject::connect(&socket, &QLocalSocket::readyRead, &app, [&] {
        input += socket.readAll();
        if (input.size() > 16384) {
            app.exit(2);
            return;
        }
        if (!input.contains('\n'))
            return;
        if (configured) {
            if (input != "commit-v1\n") {
                app.exit(2);
                return;
            }
            committed = true;
            deadline.stop();
            socket.write("committed-v1\n");
            input.clear();
            return;
        }
        const auto document = QJsonDocument::fromJson(input.trimmed());
        const auto object = document.object();
        const auto path = object.value("lock").toString();
        const auto values = object.value("arguments").toArray();
        if (!document.isObject() || !QFileInfo(path).isAbsolute() ||
            QFileInfo(path).fileName() != "instance.lock" ||
            !QFileInfo(QFileInfo(path).absolutePath()).isDir() ||
            !object.value("arguments").isArray() || values.size() > 128) {
            app.exit(2);
            return;
        }
        for (const auto &value : values) {
            if (!value.isString() || value.toString().size() > 4096 ||
                value.toString().contains(QChar::Null)) {
                app.exit(2);
                return;
            }
            arguments.append(value.toString());
        }
        lock = std::make_unique<QLockFile>(path);
        lock->setStaleLockTime(0);
        configured = true;
        input.clear();
        socket.write("ready-v1\n");
    });
    QObject::connect(&socket, &QLocalSocket::disconnected, &app, [&] {
        if (!committed) {
            app.exit(1);
            return;
        }
        deadline.start(30000);
        lockPoll.start(50);
    });
    QObject::connect(&socket, &QLocalSocket::errorOccurred, &app,
                     [&](QLocalSocket::LocalSocketError error) {
                         if (error != QLocalSocket::PeerClosedError)
                             app.exit(1);
                     });
    QObject::connect(&lockPoll, &QTimer::timeout, &app, [&] {
        if (!lock->tryLock(0)) {
            if (lock->error() != QLockFile::LockFailedError)
                app.exit(1);
            return;
        }
        // The former owner's destructors have released exclusivity. Release
        // our probe lock before the successor acquires it; a concurrent normal
        // launch is handled by SingleInstance in the successor as usual.
        lock->unlock();
        app.exit(startDetached(arguments) ? 0 : 1);
    });
    QTimer::singleShot(0, &socket, [&] { socket.connectToServer(name); });
    return app.exec();
}
} // namespace EchoMusic
