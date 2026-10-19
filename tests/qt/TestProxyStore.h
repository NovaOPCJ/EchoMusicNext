#pragma once
#include "LocalCredentialStore.h"
#include <QHash>

// Injected byte store for deterministic SQL failure/transport probes.
class TestProxyStore final : public EchoMusic::LocalCredentialStore
{
public:
    QHash<QString, QByteArray> entries;
    bool failWrite = false;
    void read(quint64 id, const QString &key) override
    {
        emit finished(id, entries.contains(key), entries.value(key));
    }
    void write(quint64 id, const QString &key, const QByteArray &bytes) override
    {
        if (!failWrite)
            entries.insert(key, bytes);
        emit finished(id, !failWrite, {});
    }
    void remove(quint64 id, const QString &key) override
    {
        entries.remove(key);
        emit finished(id, true, {});
    }
};
