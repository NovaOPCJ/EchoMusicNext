#pragma once
#include <QAbstractListModel>
#include <QJsonArray>
namespace EchoMusic
{
/// Canonical candidates only. Phone/code stay in the controller, never in this model.
class LoginCandidateModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum Role
    {
        UserId = Qt::UserRole + 1,
        Nickname,
        AvatarUrl,
        Username
    };
    explicit LoginCandidateModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : int(m_rows.size());
    }
    int count() const
    {
        return int(m_rows.size());
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool replace(const QJsonArray &candidates);
    void clear();
    quint64 userIdAt(int row) const;
signals:
    void countChanged();

private:
    struct Candidate
    {
        quint64 userId;
        QString nickname, avatarUrl, username;
    };
    QList<Candidate> m_rows;
};
} // namespace EchoMusic
