#pragma once
#include <QAbstractListModel>
#include <QVariantList>
namespace EchoMusic
{
/// Read-only projection of ten preference choices. Incremental role changes
/// keep a pressed Slider alive; replacing a QVariantList would interrupt drag.
class PreferenceStrengthModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        Id = Qt::UserRole + 1,
        Label,
        Value,
        Status,
        Editable
    };
    explicit PreferenceStrengthModel(QObject *parent) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_rows.size();
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void update(const QVariantList &rows);

private:
    QVariantList m_rows;
};
} // namespace EchoMusic
