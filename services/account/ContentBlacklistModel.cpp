#include "ContentBlacklistModel.h"
#include <QDateTime>
namespace EchoMusic
{
QHash<int, QByteArray> ContentBlacklistModel::roleNames() const
{
    return {
        {Key, "entryKey"}, {Name, "entryName"}, {Date, "createdDate"}, {Removable, "removable"}};
}
QVariant ContentBlacklistModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.model() != this || i.row() < 0 || i.row() >= count())
        return {};
    const auto row = m_rows.at(i.row()).toObject();
    switch (role) {
    case Key:
        return row.value("key").toString();
    case Name:
        return row.value("name").toString();
    case Removable:
        return !row.value("key").toString().isEmpty();
    case Date: {
        const auto text = row.value("created_at").toString().trimmed();
        bool numeric = false;
        const auto value = text.toLongLong(&numeric);
        const auto date = numeric && value > 0
                              ? (text.size() <= 10 ? QDateTime::fromSecsSinceEpoch(value)
                                                   : QDateTime::fromMSecsSinceEpoch(value))
                              : QDateTime::fromString(text, Qt::ISODate);
        return date.isValid() ? date.toString("yyyy-MM-dd") : QString();
    }
    default:
        return {};
    }
}
QJsonObject ContentBlacklistModel::entry(const QString &key) const
{
    if (key.isEmpty())
        return {};
    for (const auto &value : m_rows)
        if (value.toObject().value("key").toString() == key)
            return value.toObject();
    return {};
}
void ContentBlacklistModel::update(const QJsonArray &rows, bool append)
{
    if (!append) {
        beginResetModel();
        m_rows = rows;
        endResetModel();
        emit changed();
        return;
    }
    // Preserve offset ordering, replacing repeated keys without discarding keyless
    // provider records. Model state owns selection data, never recycled delegates.
    for (const auto &value : rows) {
        const auto key = value.toObject().value("key").toString();
        int existing = -1;
        if (!key.isEmpty())
            for (int i = 0; i < m_rows.size(); ++i)
                if (m_rows[i].toObject().value("key").toString() == key) {
                    existing = i;
                    break;
                }
        if (existing >= 0) {
            m_rows[existing] = value;
            emit dataChanged(index(existing), index(existing));
        } else {
            const auto next = count();
            beginInsertRows({}, next, next);
            m_rows.append(value);
            endInsertRows();
        }
    }
    emit changed();
}
void ContentBlacklistModel::remove(const QString &key)
{
    for (int i = 0; i < count(); ++i)
        if (m_rows[i].toObject().value("key").toString() == key) {
            beginRemoveRows({}, i, i);
            m_rows.removeAt(i);
            endRemoveRows();
            emit changed();
            return;
        }
}
} // namespace EchoMusic
