#include "LoginDeviceModel.h"
#include <QJsonObject>
#include <QDateTime>
#include <QRegularExpression>
namespace EchoMusic
{
QHash<int, QByteArray> LoginDeviceModel::roleNames() const
{
    return {{Key, "deviceKey"},      {Title, "title"},           {Detail, "detail"},
            {Activity, "activity"},  {Current, "currentDevice"}, {NewDevice, "newDevice"},
            {CanRemove, "canRemove"}};
}
QVariant LoginDeviceModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.model() != this || i.row() < 0 || i.row() >= count())
        return {};
    const auto r = m_rows[i.row()].toObject();
    const auto platform = r.value("platform").toString();
    const auto time = r.value("active_at_seconds").toInteger();
    switch (role) {
    case Key:
        return r.value("id").toString();
    case Title: {
        auto title = r.value("model").toString();
        if (title.isEmpty())
            title = (platform.contains("安卓") || platform.contains("android", Qt::CaseInsensitive))
                        ? tr("酷狗Android客户端")
                        : platform;
        return title.isEmpty() ? tr("登录设备 %1").arg(i.row() + 1) : title;
    }
    case Detail: {
        const auto type = r.value("login_type").toInt();
        const QStringList types{tr("账号密码登录"), tr("手机登录"), tr("微信登录"), tr("QQ登录"),
                                tr("苹果登录"),     tr("微博登录"), tr("扫码登录")};
        QStringList parts{r.value("login_type").isDouble() && type >= 0 && type < types.size()
                              ? types[type]
                          : r.value("login_type").isDouble() ? tr("未知(%1)").arg(type)
                                                             : tr("未知登录方式")};
        if (!platform.isEmpty())
            parts.append(platform);
        return parts.join(" · ");
    }
    case Activity: {
        QStringList parts;
        if (time > 0)
            parts.append(QDateTime::fromSecsSinceEpoch(time).toString("yyyy-MM-dd HH:mm"));
        const auto locations = r.value("location")
                                   .toString()
                                   .split(QRegularExpression("[\\s,，/]+"), Qt::SkipEmptyParts);
        if (!locations.isEmpty())
            parts.append(locations.last());
        return parts.join(" · ");
    }
    case Current:
        return r.value("current").toBool();
    case NewDevice:
        return time > 0 && r.value("new_at_seconds").toInteger() == time;
    case CanRemove:
        return removable(r);
    }
    return {};
}
QJsonObject LoginDeviceModel::device(const QString &key) const
{
    for (const auto &value : m_rows) {
        const auto row = value.toObject();
        if (row.value("id").toString() == key)
            return row;
    }
    return {};
}
bool LoginDeviceModel::removable(const QJsonObject &row)
{
    return !row.isEmpty() && !row.value("current").toBool() &&
           !row.value("id").toString().isEmpty() &&
           row.value("active_at_seconds").toInteger() > 0 && row.value("app_id").toInteger() > 0 &&
           row.value("client_version").toInteger() > 0;
}
void LoginDeviceModel::replace(QJsonArray rows)
{
    if (rows == m_rows)
        return;
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    emit changed();
}
} // namespace EchoMusic
