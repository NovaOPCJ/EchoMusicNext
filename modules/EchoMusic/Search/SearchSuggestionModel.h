#pragma once
#include "ApiClient.h"
#include <QAbstractListModel>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Titlebar-owned suggestions with 280ms debounce. QML handles IME commits.
/// Projects Rust domain fields without opening destinations.
class SearchSuggestionModel final : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by SearchModel")
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString defaultKeyword READ defaultKeyword NOTIFY changed)
    Q_PROPERTY(QStringList promotions READ promotions NOTIFY changed)
public:
    enum Role
    {
        SuggestionText = Qt::UserRole + 1,
        GroupTitle,
        GroupStart
    };
    explicit SearchSuggestionModel(ApiClient &api, QObject *parent);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool loading() const
    {
        return m_loading;
    }
    QString defaultKeyword() const
    {
        return m_defaultKeyword;
    }
    QStringList promotions() const
    {
        return m_promotions;
    }
    void initialize();
    /// Preference comes from ShellSettings; this model owns only the read and
    /// presentation lifecycle. Disable cancels late default-word responses.
    void setDefaultEnabled(bool enabled);
    void shutdown();
    Q_INVOKABLE void setQuery(const QString &query);
    Q_INVOKABLE void setActive(bool active);
    Q_INVOKABLE QString textAt(int index) const;
signals:
    void changed();

private:
    friend class SearchModelTestAccess;
    void cancelSuggestion();
    void replaceRows();
    void succeeded(quint64 id, const QByteArray &payload);
    void failed(quint64 id);
    struct Row
    {
        QString text, group;
        bool first;
    };
    ApiClient &m_api;
    QTimer m_debounce;
    QList<Row> m_rows;
    QString m_query, m_requestedQuery, m_defaultKeyword;
    QStringList m_promotions;
    quint64 m_request = 0, m_defaultRequest = 0;
    bool m_active = false, m_loading = false, m_started = false, m_closing = false;
    bool m_defaultEnabled = false;
};
} // namespace EchoMusic
