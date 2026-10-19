#pragma once
#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

namespace EchoMusic
{
/// Read-only presentation slices over the same directory. Retain source roles,
/// identity and order without cloning catalog data into another writable model.
class ArtistDirectorySliceModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int avatarLimit READ avatarLimit WRITE setAvatarLimit NOTIFY avatarLimitChanged)
    Q_PROPERTY(bool featured READ featured WRITE setFeatured NOTIFY featuredChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit ArtistDirectorySliceModel(QObject *parent = nullptr);
    void setSourceModel(QAbstractItemModel *source) override;
    int avatarLimit() const
    {
        return m_avatarLimit;
    }
    void setAvatarLimit(int value);
    bool featured() const
    {
        return m_featured;
    }
    void setFeatured(bool value);
    int count() const
    {
        return rowCount();
    }
signals:
    void avatarLimitChanged();
    void featuredChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    int m_avatarLimit = 20;
    bool m_featured = true;
    QList<QMetaObject::Connection> m_sourceConnections;
};
} // namespace EchoMusic
