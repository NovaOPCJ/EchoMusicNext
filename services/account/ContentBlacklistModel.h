#pragma once
#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
namespace EchoMusic
{
class ContentBlacklistModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY changed)
public:
    enum Role
    {
        Key = Qt::UserRole + 1,
        Name,
        Date,
        Removable
    };
    explicit ContentBlacklistModel(QObject *parent) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : count();
    }
    int count() const
    {
        return m_rows.size();
    }
    QVariant data(const QModelIndex &, int) const override;
    QHash<int, QByteArray> roleNames() const override;
    QJsonObject entry(const QString &key) const;
    void update(const QJsonArray &rows, bool append);
    void remove(const QString &key);
signals:
    void changed();

private:
    QJsonArray m_rows;
};
} // namespace EchoMusic
