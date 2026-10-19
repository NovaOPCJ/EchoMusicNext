#include "SocialUserModel.h"
#include "AccountIdentity.h"
#include <QJsonObject>
#include <QDateTime>
namespace EchoMusic
{
QHash<int, QByteArray> SocialUserModel::roleNames() const
{
    return {{Key, "entryKey"},
            {UserId, "userId"},
            {ArtistId, "artistId"},
            {Nickname, "nickname"},
            {AvatarUrl, "avatarUrl"},
            {Description, "description"},
            {Meta, "meta"},
            {Following, "following"},
            {CanUnfollow, "canUnfollow"},
            {VisitedAt, "visitedAt"},
            {RelationshipKnown, "relationshipKnown"},
            {AvatarIdentityIcon, "avatarIdentityIcon"},
            {AvatarIdentityLabel, "avatarIdentityLabel"},
            {IdentityBadges, "identityBadges"},
            {IdentityType, "identityType"}};
}
QVariant SocialUserModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.model() != this || i.row() < 0 || i.row() >= count())
        return {};
    const auto r = m_rows[i.row()].toObject();
    switch (role) {
    case Key:
        return r.value("key").toString();
    case UserId:
        return r.value("user_id").toInteger();
    case ArtistId:
        return r.value("artist_id").toInteger();
    case Nickname:
        return r.value("nickname").toString();
    case AvatarUrl:
        return r.value("avatar_url").toString();
    case AvatarIdentityIcon:
        return AccountIdentity::avatarIcon(
            AccountIdentity::fromDomain(r.value("identity").toObject()).avatarKind());
    case AvatarIdentityLabel:
        return AccountIdentity::avatarLabel(
            AccountIdentity::fromDomain(r.value("identity").toObject()).avatarKind());
    case IdentityBadges: {
        auto badges = AccountIdentity::fromDomain(r.value("identity").toObject()).badges(false);
        const auto membership = r.value("membership").toObject();
        const auto label = membership.value("super_active").toBool()    ? tr("超级VIP")
                           : membership.value("deluxe_active").toBool() ? tr("豪华VIP")
                                                                        : QString();
        if (!label.isEmpty())
            badges.prepend(QVariantMap{{"kind", "membership"}, {"label", label}});
        return badges;
    }
    case Description:
        return r.value("description").toString();
    case Following:
        return r.value("following").toBool();
    case CanUnfollow:
        return r.value("user_id").toInteger() > 0 && r.value("following").toBool();
    case RelationshipKnown:
        return r.value("user_id").toInteger() > 0 && r.value("following").isBool();
    case VisitedAt:
        return r.value("visited_at_seconds").toInteger();
    case Meta:
        if (r.value("visited_at_seconds").isDouble() &&
            r.value("visited_at_seconds").toInteger() > 0)
            return QDateTime::fromSecsSinceEpoch(r.value("visited_at_seconds").toInteger())
                .toString("yyyy-MM-dd HH:mm");
        if (r.value("user_id").toInteger() == 0 && r.value("artist_id").toInteger() > 0)
            return tr("歌手");
        if (r.value("mutual").toBool())
            return tr("互相关注");
        // Following already determines the row action; keep metadata for the
        // distinct mutual relationship, artist identity and visitor timestamp.
        // Return a typed empty string: an invalid QVariant becomes "undefined"
        // when assigned to the delegate's required string property.
        return QString();
    case IdentityType:
        // Use raw iden_type with singerid to distinguish followed singers from users.
        return r.value("identity_facts").toObject().value("identity_type").toVariant();
    }
    return {};
}
QJsonObject SocialUserModel::entry(const QString &key) const
{
    for (const auto &v : m_rows) {
        const auto o = v.toObject();
        if (o["key"].toString() == key)
            return o;
    }
    return {};
}
void SocialUserModel::replace(QJsonArray rows)
{
    // Equal snapshots produce no notifications; account/tab replacement is atomic.
    // These endpoints return complete snapshots rather than paginated data.
    if (rows == m_rows)
        return;
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    emit changed();
}
} // namespace EchoMusic
