#include "ArtistDirectorySliceModel.h"
#include <algorithm>

namespace EchoMusic
{
ArtistDirectorySliceModel::ArtistDirectorySliceModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &QAbstractItemModel::modelReset, this, &ArtistDirectorySliceModel::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this,
            &ArtistDirectorySliceModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ArtistDirectorySliceModel::countChanged);
}
void ArtistDirectorySliceModel::setSourceModel(QAbstractItemModel *source)
{
    if (sourceModel() == source)
        return;
    for (const auto &connection : m_sourceConnections)
        disconnect(connection);
    m_sourceConnections.clear();
    QSortFilterProxyModel::setSourceModel(source);
    if (!source)
        return;
    // The boundary depends on source positions, not roles. After Qt handles
    // an insertion/removal/move, previously accepted rows may cross that edge.
    const auto refreshBoundary = [this] {
        beginFilterChange();
        endFilterChange(Direction::Rows);
    };
    m_sourceConnections = {
        connect(source, &QAbstractItemModel::rowsInserted, this, refreshBoundary),
        connect(source, &QAbstractItemModel::rowsRemoved, this, refreshBoundary),
        connect(source, &QAbstractItemModel::rowsMoved, this, refreshBoundary),
        connect(source, &QAbstractItemModel::layoutChanged, this, refreshBoundary)};
}
void ArtistDirectorySliceModel::setAvatarLimit(int value)
{
    value = std::max(0, value);
    if (m_avatarLimit == value)
        return;
    beginFilterChange();
    m_avatarLimit = value;
    endFilterChange(Direction::Rows);
    emit avatarLimitChanged();
}
void ArtistDirectorySliceModel::setFeatured(bool value)
{
    if (m_featured == value)
        return;
    beginFilterChange();
    m_featured = value;
    endFilterChange(Direction::Rows);
    emit featuredChanged();
}
bool ArtistDirectorySliceModel::filterAcceptsRow(int row, const QModelIndex &parent) const
{
    return !parent.isValid() && (m_featured ? row < m_avatarLimit : row >= m_avatarLimit);
}
} // namespace EchoMusic
