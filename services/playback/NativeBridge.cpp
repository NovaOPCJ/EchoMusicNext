#include "NativeBridge.h"
#include "echo_ffi.h"
#include <QMutex>
#include <QMutexLocker>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <deque>
#include <utility>
namespace EchoMusic
{
struct NativeResult
{
    quint64 id, revision;
    bool success;
    QByteArray data;
};
struct NativeSink
{
    QMutex mutex;
    std::deque<NativeResult> results;
    NativeBridge *owner = nullptr;
    bool posted = false;
    void schedule()
    {
        // Called under mutex. shutdown clears owner under the same lock and
        // joins native producers before QObject destruction, so the context is
        // live while this notification is enqueued. Qt retires it with owner.
        QMetaObject::invokeMethod(
            owner, [target = owner] { target->drain(); }, Qt::QueuedConnection);
    }
};
static void nativeCompleted(void *context, const EchoResult *result)
{
    auto &sink = *static_cast<NativeSink *>(context);
    NativeResult value{result->request_id, result->session_revision,
                       result->status == EchoStatus::Success,
                       QByteArray(reinterpret_cast<const char *>(result->payload),
                                  qsizetype(result->payload_len))};
    QMutexLocker lock(&sink.mutex);
    if (!sink.owner)
        return;
    sink.results.push_back(std::move(value));
    if (!std::exchange(sink.posted, true))
        sink.schedule();
}
NativeBridge::NativeBridge(QObject *parent)
    : QObject(parent), m_sink(std::make_shared<NativeSink>())
{
    m_sink->owner = this;
    m_native = echo_capabilities_create(nativeCompleted, m_sink.get());
}
void NativeBridge::drain()
{
    std::deque<NativeResult> results;
    {
        QMutexLocker lock(&m_sink->mutex);
        results.swap(m_sink->results);
        m_sink->posted = false;
    }
    const QPointer<NativeBridge> guard(this);
    for (const auto &result : results) {
        if (m_closing)
            return;
        if (m_pending.remove(result.id)) {
            emit completed(result.id, result.revision, result.success, result.data);
            // Completion may synchronously retire the owning application service.
            if (!guard)
                return;
        }
    }
}
NativeBridge::~NativeBridge()
{
    shutdown();
    if (m_destroy)
        m_destroy->wait();
}
quint64 NativeBridge::submit(Operation operation, const QJsonValue &data, quint64 revision)
{
    if (m_closing || !m_native || m_pending.size() >= 64)
        return 0;
    // QJsonDocument cannot represent scalar JSON; remove the enclosing array.
    auto bytes = QJsonDocument(QJsonArray{data}).toJson(QJsonDocument::Compact);
    bytes = bytes.mid(1, bytes.size() - 2);
    const auto id = echo_capabilities_submit(m_native, quint32(operation),
                                             reinterpret_cast<const uint8_t *>(bytes.constData()),
                                             size_t(bytes.size()), revision);
    if (id)
        m_pending.insert(id);
    return id;
}
bool NativeBridge::cancel(quint64 id)
{
    if (m_closing || !m_native || !m_pending.remove(id))
        return false;
    // Native may have finished while its copied callback is still waiting in
    // the GUI sink. Removing GUI ownership also cancels that pending delivery.
    echo_capabilities_cancel(m_native, id);
    return true;
}
void NativeBridge::shutdown()
{
    if (std::exchange(m_closing, true))
        return;
    {
        QMutexLocker lock(&m_sink->mutex);
        m_sink->owner = nullptr;
        m_sink->results.clear();
    }
    m_pending.clear();
    auto *native = std::exchange(m_native, nullptr);
    // Joined before callback context is freed. Qt removes queued notifications
    // on object destruction; no native callback targets the QObject directly.
    m_destroy = QThread::create([native, sink = m_sink] { echo_capabilities_destroy(native); });
    connect(m_destroy, &QThread::finished, this, &NativeBridge::closed);
    m_destroy->setParent(this);
    m_destroy->start();
}
} // namespace EchoMusic
