#pragma once
#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
namespace EchoMusic
{
class LoginDeviceModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
public:
    enum Role
    {
        Key = Qt::UserRole + 1,
        Title,
        Detail,
        Activity,
        Current,
        NewDevice,
        CanRemove
    };
    explicit LoginDeviceModel(QObject *parent) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &p = {}) const override
    {
        return p.isValid() ? 0 : count();
    }
    int count() const
    {
        return m_rows.size();
    }
    QVariant data(const QModelIndex &, int) const override;
    QHash<int, QByteArray> roleNames() const override;
    void replace(QJsonArray rows);
    QJsonObject device(const QString &key) const;
    static bool removable(const QJsonObject &device);

private:
    QJsonArray m_rows;
signals:
    void changed();
};
} // namespace EchoMusic
