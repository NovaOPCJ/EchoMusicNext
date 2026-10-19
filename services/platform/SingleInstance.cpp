#include "SingleInstance.h"
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>

namespace EchoMusic
{
namespace
{
const QByteArray request = "activate-v1\n";
const QByteArray accepted = "accepted-v1\n";
} // namespace
SingleInstance::SingleInstance(QObject *parent) : QObject(parent)
{
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    m_server.setMaxPendingConnections(16);
    connect(&m_server, &QLocalServer::newConnection, this, &SingleInstance::acceptConnections);
}
SingleInstance::~SingleInstance()
{
    // Close the endpoint before releasing exclusivity, so a successor cannot
    // have its new socket removed by the previous owner's cleanup.
    m_server.close();
    m_lock.reset();
}
SingleInstance::Result SingleInstance::start(const QString &dataDirectory, int timeoutMs,
                                             const QUrl &openUrl)
{
    Q_ASSERT(!m_lock);
    if (dataDirectory.isEmpty() || !QDir().mkpath(dataDirectory)) {
        m_error = tr("无法创建应用实例目录");
        return Result::Failed;
    }
    const auto directory = QFileInfo(dataDirectory).canonicalFilePath();
    const auto digest = QCryptographicHash::hash(directory.toUtf8(), QCryptographicHash::Sha256);
    const auto name =
        QStringLiteral("echomusicnext-") + QString::fromLatin1(digest.toHex().left(24));
    m_lockPath = directory + QStringLiteral("/instance.lock");
    m_lock = std::make_unique<QLockFile>(m_lockPath);
    // A live music player can hold this for days. Age alone must never steal it;
    // Qt still detects dead owner PIDs and recovers locks left by crashes.
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock(0)) {
        if (m_lock->error() != QLockFile::LockFailedError) {
            m_error = tr("无法取得应用实例锁");
            return Result::Failed;
        }
        const auto payload =
            openUrl.isEmpty() ? request : QByteArray("open-url-v1 ") + openUrl.toEncoded() + '\n';
        if (payload.size() <= 8192 && activate(name, timeoutMs, payload))
            return Result::Activated;
        m_error = tr("已有实例未响应，请稍后重试");
        return Result::Failed;
    }
    // Only the exclusive owner may remove a stale endpoint after a crash.
    if (!m_server.listen(name)) {
        QLocalServer::removeServer(name);
        if (!m_server.listen(name)) {
            m_error = m_server.errorString();
            m_lock.reset();
            return Result::Failed;
        }
    }
    return Result::Primary;
}
bool SingleInstance::activate(const QString &name, int timeoutMs, const QByteArray &payload)
{
    // Bounded wait only in the secondary process, before business initialization.
    // The lock owner may still be between acquisition and listen() at launch.
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeoutMs) {
        QLocalSocket socket;
        socket.connectToServer(name);
        const auto remaining = [&] { return qMax(1, timeoutMs - int(elapsed.elapsed())); };
        if (!socket.waitForConnected(qMin(remaining(), 100))) {
            QThread::msleep(20);
            continue;
        }
        if (socket.write(payload) != payload.size())
            return false;
        socket.flush();
        QByteArray response;
        while (elapsed.elapsed() < timeoutMs && response.size() < accepted.size()) {
            if (!socket.bytesAvailable() && !socket.waitForReadyRead(remaining()))
                break;
            response += socket.read(accepted.size() - response.size());
        }
        return response == accepted;
    }
    return false;
}
void SingleInstance::acceptConnections()
{
    while (m_server.hasPendingConnections()) {
        auto *socket = m_server.nextPendingConnection();
        if (!socket)
            continue;
        auto *deadline = new QTimer(socket);
        deadline->setSingleShot(true);
        connect(deadline, &QTimer::timeout, socket, &QLocalSocket::abort);
        deadline->start(2000);
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        const auto consume = [this, socket, deadline] {
            // Bounded URL-only protocol. The share coordinator validates the complete target;
            // no credentials or executable commands cross the instance channel.
            const auto bytes = socket->peek(socket->bytesAvailable());
            const QByteArray prefix("open-url-v1 ");
            if (bytes.size() > 8192 || (!request.startsWith(bytes) && !bytes.startsWith(request) &&
                                        !prefix.startsWith(bytes) && !bytes.startsWith(prefix))) {
                socket->abort();
                return;
            }
            if (!bytes.endsWith('\n'))
                return;
            const auto payload = socket->readAll();
            if (payload != request) {
                if (!payload.startsWith(prefix) || payload.count('\n') != 1 ||
                    payload.contains('\r')) {
                    socket->abort();
                    return;
                }
                const auto url = QUrl::fromEncoded(
                    payload.mid(prefix.size(), payload.size() - prefix.size() - 1),
                    QUrl::StrictMode);
                const bool mod = url.isLocalFile() &&
                                 url.path().endsWith(".echomod", Qt::CaseInsensitive) &&
                                 !url.hasQuery() && !url.hasFragment();
                if (!url.isValid() || (!mod && url.scheme() != "echomusicnext") ||
                    !url.userInfo().isEmpty() || url.port() != -1) {
                    socket->abort();
                    return;
                }
                emit urlRequested(url);
            }
            deadline->stop();
            emit activationRequested();
            socket->write(accepted);
            socket->disconnectFromServer();
        };
        connect(socket, &QLocalSocket::readyRead, this, consume);
        consume();
    }
}
} // namespace EchoMusic
