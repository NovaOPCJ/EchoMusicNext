#pragma once
#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Immutable presentation records. Filtering and installation state belong to
/// the application manager; delegates never maintain their own business copy.
class PluginListModel final : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by PluginManager")
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : int(m_rows.size());
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        return index.isValid() && index.row() >= 0 && index.row() < m_rows.size() &&
                       role == Qt::UserRole + 1
                   ? m_rows[index.row()]
                   : QVariant();
    }
    QHash<int, QByteArray> roleNames() const override
    {
        return {{Qt::UserRole + 1, "pluginRecord"}};
    }
    void replace(QVariantList rows)
    {
        beginResetModel();
        m_rows = std::move(rows);
        endResetModel();
        emit changed();
    }
signals:
    void changed();

private:
    QVariantList m_rows;
};
} // namespace EchoMusic
