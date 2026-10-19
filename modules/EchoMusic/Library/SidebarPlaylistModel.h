#pragma once
#include "UserPlaylistModel.h"
#include <QSortFilterProxyModel>
#include <QSet>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Canonical projection, shared by expanded/rail sidebars. No QML array copies.
class SidebarPlaylistModel final : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by library controller")
    Q_PROPERTY(int count READ count NOTIFY changed)
public:
    explicit SidebarPlaylistModel(UserPlaylistModel &source, QObject *parent = nullptr);
    int count() const
    {
        return rowCount();
    }
    void setCollected(bool value);
    /// Derived layout filter only; canonical catalog and server data stay intact.
    void setHiddenDefaultKinds(const QSet<quint32> &kinds);
    int sourceRow(quint64 cloudId) const;
signals:
    void changed();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    bool m_collected = false;
    QSet<quint32> m_hiddenDefaultKinds;
};
} // namespace EchoMusic
