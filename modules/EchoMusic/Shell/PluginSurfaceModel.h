#pragma once
#include <QObject>
#include <QPointer>
#include <QMap>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Host-owned bounded declarative contributions. Contains only package URLs and
/// scoped SDK objects; Shell never depends on the plugin runtime module.
class PluginSurfaceModel final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by ShellController")
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
public:
    explicit PluginSurfaceModel(QObject *parent = nullptr) : QObject(parent) {}
    QVariantList entries() const;
    void insert(const QString &key, const QString &target, const QUrl &component, QObject *scope);
    void remove(const QString &key);
signals:
    void changed();

private:
    struct Entry
    {
        QString target;
        QUrl component;
        QPointer<QObject> scope;
    };
    QMap<QString, Entry> m_entries;
};
} // namespace EchoMusic
