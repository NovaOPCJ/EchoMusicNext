#pragma once
#include <QAbstractListModel>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Ephemeral dialog navigation/filter; business preferences stay in their owners.
/// Entries without a functional provider remain disabled, never load a stub page.
class SettingsCatalog : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed)
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(QUrl source READ source NOTIFY changed)
    Q_PROPERTY(
        bool playbackAvailable READ playbackAvailable WRITE setPlaybackAvailable NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
public:
    enum Role
    {
        Key = Qt::UserRole + 1,
        Label,
        Glyph,
        Available
    };
    explicit SettingsCatalog(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString filter() const
    {
        return m_filter;
    }
    QString current() const
    {
        return m_current;
    }
    QUrl source() const;
    int count() const
    {
        return int(m_visible.size());
    }
    bool playbackAvailable() const
    {
        return m_playbackAvailable;
    }
    void setPlaybackAvailable(bool value);
    void setFilter(const QString &filter);
    Q_INVOKABLE void select(const QString &key);
    Q_INVOKABLE void reset();
signals:
    void changed();

private:
    struct Entry
    {
        QString key, label, glyph, keywords, component;
    };
    bool available(const Entry &entry) const;
    bool m_playbackAvailable = false;
    QList<Entry> m_entries;
    QList<int> m_visible;
    QString m_filter, m_current = QStringLiteral("interface");
};
} // namespace EchoMusic
