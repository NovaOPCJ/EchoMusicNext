#include "ListenTogetherListModel.h"
namespace EchoMusic
{
QVariant ListenTogetherListModel::data(const QModelIndex &index, int role) const
{
    return role == Qt::UserRole + 1 && index.isValid() && index.row() >= 0 &&
                   index.row() < m_rows.size()
               ? m_rows[index.row()]
               : QVariant();
}
QVariantMap ListenTogetherListModel::item(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows[row].toMap() : QVariantMap();
}
QString ListenTogetherListModel::identity(const QVariant &row, int index)
{
    const auto data = row.toMap();
    const auto id = data.value("key", data.value("id")).toString();
    // An anonymous row still exists. This internal view key never becomes a
    // provider user/room/song identity or a navigable resource identifier.
    return id.isEmpty() ? QString("anonymous-row:%1").arg(index) : id;
}
void ListenTogetherListModel::replace(const QVariantList &rows)
{
    if (m_rows == rows)
        return;
    for (int i = 0; i < rows.size(); ++i) {
        const auto key = identity(rows[i], i);
        int existing = -1;
        for (int n = i; n < m_rows.size(); ++n)
            if (identity(m_rows[n], n) == key) {
                existing = n;
                break;
            }
        if (existing < 0) {
            beginInsertRows({}, i, i);
            m_rows.insert(i, rows[i]);
            endInsertRows();
        } else {
            if (existing != i) {
                beginMoveRows({}, existing, existing, {}, i);
                m_rows.move(existing, i);
                endMoveRows();
            }
            if (m_rows[i] != rows[i]) {
                m_rows[i] = rows[i];
                emit dataChanged(index(i), index(i), {Qt::UserRole + 1});
            }
        }
    }
    if (m_rows.size() > rows.size()) {
        beginRemoveRows({}, rows.size(), m_rows.size() - 1);
        m_rows.erase(m_rows.begin() + rows.size(), m_rows.end());
        endRemoveRows();
    }
    emit changed();
}
} // namespace EchoMusic
