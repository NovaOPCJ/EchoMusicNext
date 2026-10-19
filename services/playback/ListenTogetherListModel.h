#pragma once
#include <QAbstractListModel>
#include <QVariantMap>
#include <QVariantList>
namespace EchoMusic
{
/// Read-only presentation rows. The room controller alone applies snapshots;
/// Qt virtual views own delegates and never become another business store.
class ListenTogetherListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
public:
    explicit ListenTogetherListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_rows.size();
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override
    {
        return {{Qt::UserRole + 1, "rowData"}};
    }
    Q_INVOKABLE QVariantMap item(int row) const;
    void replace(const QVariantList &rows);
    const QVariantList &rows() const
    {
        return m_rows;
    }
    void clear()
    {
        replace({});
    }
signals:
    void changed();

private:
    static QString identity(const QVariant &row, int index);
    QVariantList m_rows;
};
} // namespace EchoMusic
