#include "HttpRequestBody.h"
#include <QDir>
#include <QSet>
#include <limits>

namespace EchoMusic
{
namespace
{
constexpr qint64 memoryLimit = 1024 * 1024;
constexpr qsizetype lineLimit = 64 * 1024;
} // namespace
bool HttpRequestBody::reject(int status)
{
    m_error = status;
    clear();
    return false;
}
void HttpRequestBody::clear()
{
    m_file.reset();
    m_directory.reset();
    m_memory.close();
    m_memory.buffer().clear();
    m_pending.clear();
}
bool HttpRequestBody::begin(const Headers &headers)
{
    bool hasLength = false, chunked = false;
    for (const auto &[name, value] : headers) {
        if (name.toLower() == "content-length") {
            bool valid = false;
            const auto length = value.toLongLong(&valid);
            for (const auto c : value)
                if (c < '0' || c > '9')
                    valid = false;
            if (!valid || length < 0 || (hasLength && length != m_remaining))
                return reject(400);
            m_remaining = length;
            hasLength = true;
        } else if (name.toLower() == "transfer-encoding") {
            if (chunked || value.toLower() != "chunked")
                return reject(501);
            chunked = true;
        }
    }
    if (chunked && hasLength)
        return reject(400);
    m_length = chunked ? -1 : m_remaining;
    m_state = chunked ? State::Size : m_remaining ? State::Fixed : State::Complete;
    return m_memory.open(QIODevice::ReadWrite) || reject(507);
}
bool HttpRequestBody::write(const QByteArray &bytes)
{
    if (bytes.size() > std::numeric_limits<qint64>::max() - m_size)
        return reject(413);
    if (!m_file && m_size + bytes.size() > memoryLimit) {
        // QTemporaryDir creates owner-only access atomically, including an
        // inheritable Windows ACL. QFile::setPermissions alone only changes
        // the Windows read-only flag and cannot secure an inherited temp ACL.
        auto directory =
            std::make_unique<QTemporaryDir>(QDir::tempPath() + "/echomusic-http-XXXXXX");
        if (!directory->isValid())
            return reject(507);
        auto file = std::make_unique<QTemporaryFile>(directory->filePath("body-XXXXXX"));
        if (!file->open() ||
            !file->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
            file->write(m_memory.data()) != m_memory.size())
            return reject(507);
        m_directory = std::move(directory);
        m_file = std::move(file);
        m_memory.close();
        m_memory.buffer().clear();
    }
    auto *device = m_file ? static_cast<QIODevice *>(m_file.get()) : &m_memory;
    if (!device->seek(m_size) || device->write(bytes) != bytes.size())
        return reject(507);
    m_size += bytes.size();
    return true;
}
bool HttpRequestBody::append(const QByteArray &bytes)
{
    if (m_error)
        return false;
    if (m_state == State::Fixed) {
        if (bytes.size() > m_remaining)
            return reject(400);
        if (!write(bytes))
            return false;
        m_remaining -= bytes.size();
        if (!m_remaining)
            m_state = State::Complete;
        return true;
    }
    m_pending += bytes;
    while (!m_pending.isEmpty()) {
        if (m_state == State::Complete)
            return reject(400);
        if (m_state == State::Data) {
            const auto count = qMin<qint64>(m_remaining, m_pending.size());
            if (!write(m_pending.left(count)))
                return false;
            m_pending.remove(0, count);
            m_remaining -= count;
            if (!m_remaining)
                m_state = State::DataEnd;
            continue;
        }
        if (m_state == State::DataEnd) {
            if (m_pending.size() < 2)
                return true;
            if (!m_pending.startsWith("\r\n"))
                return reject(400);
            m_pending.remove(0, 2);
            m_state = State::Size;
            continue;
        }
        const auto end = m_pending.indexOf("\r\n");
        if (end < 0)
            return m_pending.size() <= lineLimit || reject(431);
        if (end > lineLimit)
            return reject(431);
        const auto line = m_pending.left(end);
        m_pending.remove(0, end + 2);
        if (m_state == State::Size) {
            const auto size = line.first(line.indexOf(';') < 0 ? line.size() : line.indexOf(';'));
            bool valid = !size.isEmpty();
            for (const auto c : size)
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                    valid = false;
            bool fits = false;
            m_remaining = size.toLongLong(&fits, 16);
            if (!valid || !fits || m_remaining < 0)
                return reject(400);
            m_state = m_remaining ? State::Data : State::Trailers;
        } else {
            m_trailerBytes += line.size() + 2;
            if (m_trailerBytes > lineLimit)
                return reject(431);
            if (line.isEmpty()) {
                m_state = State::Complete;
                continue;
            }
            const auto colon = line.indexOf(':');
            const auto name = line.left(colon).toLower();
            const QSet<QByteArray> forbidden{"content-length",
                                             "transfer-encoding",
                                             "host",
                                             "connection",
                                             "proxy-connection",
                                             "proxy-authorization",
                                             "authorization",
                                             "cookie",
                                             "trailer",
                                             "expect",
                                             "content-type",
                                             "content-encoding",
                                             "range"};
            if (colon <= 0 || forbidden.contains(name) || line.contains('\0'))
                return reject(400);
            for (const auto c : name)
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                      QByteArray("!#$%&'*+-.^_`|~").contains(c)))
                    return reject(400);
            const auto value = line.mid(colon + 1).trimmed();
            for (const auto c : value)
                if ((quint8(c) < 32 && c != '\t') || c == 127)
                    return reject(400);
            m_trailers.append({name, value});
        }
    }
    return true;
}
bool HttpRequestBody::complete() const
{
    return !m_error && m_state == State::Complete;
}
QIODevice *HttpRequestBody::rewind()
{
    auto *device = m_file ? static_cast<QIODevice *>(m_file.get()) : &m_memory;
    if (!complete() || !device->seek(0))
        return nullptr;
    return device;
}
qint64 HttpRequestBody::readAt(qint64 offset, char *bytes, qint64 maximum)
{
    if (m_error || offset < 0 || offset > m_size || maximum < 0)
        return -1;
    auto *device = m_file ? static_cast<QIODevice *>(m_file.get()) : &m_memory;
    if (!device->seek(offset))
        return -1;
    const auto read = device->read(bytes, qMin(maximum, m_size - offset));
    if (!device->seek(m_size))
        return -1;
    return read;
}
} // namespace EchoMusic
