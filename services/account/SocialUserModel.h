#pragma once
#include <QAbstractListModel>
#include <QJsonArray>
namespace EchoMusic
{
/// Canonical Rust social rows, retained per tab; only viewport delegates live.
class SocialUserModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
public:
    enum Role
    {
        Key = Qt::UserRole + 1,
        UserId,
        ArtistId,
        Nickname,
        AvatarUrl,
        Description,
        Meta,
        Following,
        CanUnfollow,
        RelationshipKnown,
        VisitedAt,
        AvatarIdentityIcon,
        AvatarIdentityLabel,
        IdentityBadges,
        IdentityType
    };
    explicit SocialUserModel(QObject *parent) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : count();
    }
    int count() const
    {
        return m_rows.size();
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(QJsonArray rows);
    QJsonArray rows() const
    {
        return m_rows;
    }
    QJsonObject entry(const QString &key) const;

private:
    QJsonArray m_rows;
signals:
    void changed();
};
} // namespace EchoMusic
