#pragma once
#include <QSslSocket>

namespace EchoMusic
{
/// QSslSocket counts plaintext and encrypted wire queues separately. Both must
/// participate in backpressure; TLS record overhead may exceed the byte budget
/// slightly while encrypting the last admitted plaintext record.
inline qint64 pendingSocketWrite(const QTcpSocket *socket)
{
    if (!socket)
        return 0;
    const auto *ssl = qobject_cast<const QSslSocket *>(socket);
    return socket->bytesToWrite() + (ssl && ssl->isEncrypted() ? ssl->encryptedBytesToWrite() : 0);
}
} // namespace EchoMusic
