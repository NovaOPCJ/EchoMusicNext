#pragma once
#include "NativeBridge.h"
#include <QFutureWatcher>
#include <QUrl>
#include <atomic>

namespace EchoMusic
{
/// Local imports probe through the native decoder, then copy/hash off the GUI
/// thread. This owns no catalog state; the host registers only completed files.
class ImpulseResponseImporter final : public QObject
{
    Q_OBJECT
public:
    ImpulseResponseImporter(NativeBridge &native, QString directory, QObject *parent = nullptr);
    ~ImpulseResponseImporter() override;
    bool busy() const
    {
        return m_busy;
    }
    void importFiles(const QList<QUrl> &urls);
    void shutdown();
signals:
    void changed();
    void imported(const QString &path, const QString &name);
    void finished(int count, const QString &error);
    void closed();

private:
    struct Result
    {
        QString path, name, error;
    };
    void next();
    void closeIfIdle();
    NativeBridge &m_native;
    QString m_directory, m_source;
    QList<QUrl> m_urls;
    QStringList m_errors;
    QFutureWatcher<Result> m_copy;
    std::shared_ptr<std::atomic_bool> m_cancel = std::make_shared<std::atomic_bool>(false);
    quint64 m_probe = 0;
    int m_count = 0;
    bool m_busy = false, m_closing = false, m_closed = false;
};
} // namespace EchoMusic
