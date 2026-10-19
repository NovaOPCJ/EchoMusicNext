#pragma once
#include <QList>
#include <QMetaType>
#include <QString>
namespace EchoMusic
{
/// Application-local SQLite account record. Credentials stay in C++, never QML roles.
struct StoredAccount
{
    quint64 userId = 0;
    QString nickname, avatarUrl, token;
    qint64 lastUsedAt = 0;
    // Presentation metadata is independent of the saved authorization.
    QString avatarIdentityKind = {};
    QString refreshCredential = {};
    qint64 expiresAtSeconds = 0;
    bool operator==(const StoredAccount &) const = default;
};
using StoredAccounts = QList<StoredAccount>;
} // namespace EchoMusic
Q_DECLARE_METATYPE(EchoMusic::StoredAccount)
Q_DECLARE_METATYPE(EchoMusic::StoredAccounts)
