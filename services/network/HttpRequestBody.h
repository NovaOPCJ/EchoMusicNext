#pragma once
#include <QBuffer>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <memory>

namespace EchoMusic
{
/// Request-scoped rewindable upload. Small API bodies remain in memory; large
/// uploads spill to an owner-only auto-removed file on the network worker.
/// HTTP transfer chunking is decoded, without changing content encoding.
class HttpRequestBody final
{
public:
    using Headers = QList<QPair<QByteArray, QByteArray>>;
    bool begin(const Headers &headers);
    bool append(const QByteArray &bytes);
    bool complete() const;
    int errorStatus() const
    {
        return m_error;
    }
    qint64 size() const
    {
        return m_size;
    }
    QIODevice *rewind();
    qint64 expectedSize() const
    {
        return m_length;
    }
    qint64 readAt(qint64 offset, char *bytes, qint64 maximum);
    const Headers &trailers() const
    {
        return m_trailers;
    }
    void clear();

private:
    enum class State
    {
        Fixed,
        Size,
        Data,
        DataEnd,
        Trailers,
        Complete
    };
    bool write(const QByteArray &bytes);
    bool reject(int status);
    QBuffer m_memory;
    std::unique_ptr<QTemporaryDir> m_directory;
    std::unique_ptr<QTemporaryFile> m_file;
    QByteArray m_pending;
    Headers m_trailers;
    qint64 m_remaining = 0, m_size = 0, m_length = 0;
    qsizetype m_trailerBytes = 0;
    State m_state = State::Complete;
    int m_error = 0;
};
} // namespace EchoMusic
