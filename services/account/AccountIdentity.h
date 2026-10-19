#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QVariantList>
namespace EchoMusic
{
/// Display projection of canonical Rust identity. No upstream field probing or
/// writable QML state; AccountService owns the current account's sole instance.
struct AccountIdentity
{
    quint32 talentBits = 0;
    bool actor = false, business = false, star = false, singer = false;
    QStringList authorizations, talentAuthorizations, singerAuthorizations;
    static AccountIdentity fromDomain(const QJsonObject &object);
    QVariantList badges(bool student) const;
    QString avatarKind() const;
    static QString avatarIcon(const QString &kind);
    static QString avatarLabel(const QString &kind);
};
} // namespace EchoMusic
