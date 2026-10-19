#pragma once
#include "ShareTarget.h"
#include <QObject>
#include <QVariantMap>

namespace EchoMusic
{
class AccountService;
/// Application-owned publisher. Metadata is read from accepted native domain
/// projections; it owns neither a playback queue nor another account snapshot.
class SharingCommands final : public QObject
{
    Q_OBJECT
public:
    explicit SharingCommands(AccountService &, QObject *parent = nullptr);
    bool shareSong(const QString &hash, const QString &title, quint64 albumId, quint64 songId);
    Q_INVOKABLE bool sharePlaylist(const QVariantMap &detail, const QString &fallbackId = {});
    Q_INVOKABLE bool shareArtist(const QString &id, const QString &title);
    Q_INVOKABLE bool shareAlbum(const QString &id, const QString &title);
    Q_INVOKABLE bool shareRoom(const QString &id, const QString &title);
    bool copyTarget(const ShareTarget &, const QString &title = {});
    QString sharerName() const;
signals:
    /// Sent before touching the clipboard, so our own copy never offers itself
    /// back to the user as a newly received invitation.
    void copied(const QString &text);
    void notice(const QString &message, bool warning);

private:
    AccountService &m_account;
};
QString playlistShareId(const QVariantMap &detail, const QString &fallbackId = {});
} // namespace EchoMusic
