#pragma once
#include <QAbstractListModel>
#include <QPointer>
#include <algorithm>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Groups bounded card rows for a virtualized ListView. Cards share a row height
/// from their actual implicit content; no fixed-height grid truncates warnings.
class PluginGridModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY changed)
    QML_ELEMENT
    Q_PROPERTY(QAbstractItemModel *source READ source WRITE setSource NOTIFY changed)
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY changed)
public:
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() || !m_source ? 0
                                             : (m_source->rowCount() + m_columns - 1) / m_columns;
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!m_source || !index.isValid() || index.row() < 0 || index.row() >= rowCount() ||
            role != Qt::UserRole + 1)
            return {};
        QVariantList cards;
        const auto roles = m_source->roleNames();
        const int sourceRole = roles.key("pluginRecord");
        for (int i = index.row() * m_columns;
             i < std::min((index.row() + 1) * m_columns, m_source->rowCount()); ++i)
            cards.append(m_source->data(m_source->index(i, 0), sourceRole));
        return cards;
    }
    QHash<int, QByteArray> roleNames() const override
    {
        return {{Qt::UserRole + 1, "cards"}};
    }
    QAbstractItemModel *source() const
    {
        return m_source;
    }
    int columns() const
    {
        return m_columns;
    }
    void setSource(QAbstractItemModel *source)
    {
        if (m_source == source)
            return;
        if (m_source)
            disconnect(m_source, nullptr, this, nullptr);
        beginResetModel();
        m_source = source;
        endResetModel();
        if (source) {
            connect(source, &QAbstractItemModel::modelReset, this, &PluginGridModel::reset);
            connect(source, &QAbstractItemModel::rowsInserted, this, &PluginGridModel::reset);
            connect(source, &QAbstractItemModel::rowsRemoved, this, &PluginGridModel::reset);
            connect(source, &QAbstractItemModel::dataChanged, this,
                    [this](const QModelIndex &first, const QModelIndex &last) {
                        emit dataChanged(index(first.row() / m_columns),
                                         index(last.row() / m_columns));
                    });
        }
        emit changed();
    }
    void setColumns(int value)
    {
        value = std::clamp(value, 1, 8);
        if (value == m_columns)
            return;
        beginResetModel();
        m_columns = value;
        endResetModel();
        emit changed();
    }
signals:
    void changed();

private:
    void reset()
    {
        beginResetModel();
        endResetModel();
        emit changed();
    }
    QPointer<QAbstractItemModel> m_source;
    int m_columns = 1;
};
} // namespace EchoMusic
