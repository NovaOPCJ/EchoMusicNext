#include "LoginCandidateModel.h"
#include <QJsonObject>
#include <QSet>
namespace EchoMusic
{
QVariant LoginCandidateModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const auto &row = m_rows.at(index.row());
    switch (role) {
    case UserId:
        return QVariant::fromValue(row.userId);
    case Nickname:
        return row.nickname;
    case AvatarUrl:
        return row.avatarUrl;
    case Username:
        return row.username;
    default:
        return {};
    }
}
QHash<int, QByteArray> LoginCandidateModel::roleNames() const
{
    return {{UserId, "userId"},
            {Nickname, "nickname"},
            {AvatarUrl, "avatarUrl"},
            {Username, "username"}};
}
bool LoginCandidateModel::replace(const QJsonArray &candidates)
{
    if (candidates.isEmpty() || candidates.size() > 1024)
        return false;
    QList<Candidate> rows;
    QSet<quint64> seen;
    for (const auto &value : candidates) {
        const auto item = value.toObject();
        const auto id = item.value("user_id").toInteger();
        if (id <= 0 || seen.contains(quint64(id)) || !item.value("nickname").isString() ||
            !item.value("avatar_url").isString() || !item.value("username").isString())
            return false;
        seen.insert(quint64(id));
        rows.append({quint64(id), item.value("nickname").toString(),
                     item.value("avatar_url").toString(), item.value("username").toString()});
    }
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    emit countChanged();
    return true;
}
void LoginCandidateModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
    emit countChanged();
}
quint64 LoginCandidateModel::userIdAt(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows.at(row).userId : 0;
}
} // namespace EchoMusic
