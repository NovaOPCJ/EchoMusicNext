#include "PreferenceStrengthModel.h"
namespace EchoMusic
{
QHash<int, QByteArray> PreferenceStrengthModel::roleNames() const
{
    return {{Id, "entryId"},
            {Label, "choiceLabel"},
            {Value, "strengthValue"},
            {Status, "strengthStatus"},
            {Editable, "strengthEditable"}};
}
QVariant PreferenceStrengthModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.model() != this || i.row() < 0 || i.row() >= m_rows.size())
        return {};
    const auto row = m_rows[i.row()].toMap();
    switch (role) {
    case Id:
        return row.value("id");
    case Label:
        return row.value("label");
    case Value:
        return row.value("value");
    case Status:
        return row.value("status");
    case Editable:
        return row.value("editable");
    default:
        return {};
    }
}
void PreferenceStrengthModel::update(const QVariantList &rows)
{
    if (m_rows.size() != rows.size()) {
        beginResetModel();
        m_rows = rows;
        endResetModel();
        return;
    }
    for (int i = 0; i < rows.size(); ++i) {
        if (rows[i] == m_rows[i])
            continue;
        m_rows[i] = rows[i];
        emit dataChanged(index(i), index(i), {Value, Status, Editable});
    }
}
} // namespace EchoMusic
