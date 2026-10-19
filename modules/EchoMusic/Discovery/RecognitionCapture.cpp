#include "RecognitionCapture.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPermission>
namespace EchoMusic
{
NativeRecognitionCapture::NativeRecognitionCapture(NativeBridge &native, QObject *parent)
    : RecognitionCapture(parent), m_native(native)
{
    connect(
        &native, &NativeBridge::completed, this,
        [this](quint64 id, quint64 generation, bool success, const QByteArray &bytes) {
            if (generation != m_generation)
                return;
            const auto data = QJsonDocument::fromJson(bytes).object();
            QString error = success ? QString() : data.value("error").toString(tr("音频采集失败"));
            if (id == m_start) {
                m_start = 0;
                if (success && !data.value("running").toBool())
                    error = data.value("error").toString(tr("音频采集未启动"));
                if (!error.isEmpty()) {
                    m_native.submit(NativeBridge::CaptureCancel, QJsonObject{}, generation);
                    m_owned = false;
                }
                emit started(generation, error);
            } else if (id == m_stop) {
                m_stop = 0;
                m_owned = false;
                QByteArray pcm;
                if (success) {
                    const auto array = data.value("data").toArray();
                    if (array.size() > 160000)
                        error = tr("采集音频超出十秒上限");
                    else {
                        pcm.reserve(array.size());
                        for (const auto &value : array) {
                            if (!value.isDouble() || value.toInt(-1) < 0 || value.toInt() > 255) {
                                error = tr("音频采集数据无效");
                                break;
                            }
                            pcm.append(char(value.toInt()));
                        }
                    }
                    if (pcm.isEmpty() || pcm.size() % 2)
                        error = tr("no samples");
                }
                emit captured(generation, error.isEmpty() ? pcm : QByteArray(), error);
            }
        });
}
NativeRecognitionCapture::~NativeRecognitionCapture()
{
    cancel();
}
void NativeRecognitionCapture::start(quint64 generation, const QString &source,
                                     const QString &device)
{
    cancel();
    m_generation = generation;
    if (source == "mic") {
        const QMicrophonePermission permission;
        const auto status = qApp->checkPermission(permission);
        if (status == Qt::PermissionStatus::Denied) {
            emit started(generation, tr("microphone permission denied"));
            return;
        }
        if (status == Qt::PermissionStatus::Undetermined) {
            qApp->requestPermission(
                permission, this, [this, generation, source, device](const QPermission &value) {
                    if (generation != m_generation)
                        return;
                    if (value.status() != Qt::PermissionStatus::Granted)
                        emit started(generation, tr("microphone permission denied"));
                    else
                        begin(generation, source, device);
                });
            return;
        }
    }
    begin(generation, source, device);
}
void NativeRecognitionCapture::begin(quint64 generation, const QString &source,
                                     const QString &device)
{
    QJsonObject request{{"source", source == "mic" ? "input" : "system"},
                        {"max_buffer_duration_ms", 15000}};
    if (source == "mic" && !device.isEmpty() && device != "default")
        request.insert("device_id", device);
    m_owned = true;
    m_start = m_native.submit(NativeBridge::CaptureStart, request, generation);
    if (!m_start) {
        m_owned = false;
        emit started(generation, tr("音频采集服务已关闭"));
    }
}
void NativeRecognitionCapture::stop(quint64 generation)
{
    if (!m_owned || generation != m_generation) {
        emit captured(generation, {}, tr("audio capture is not running"));
        return;
    }
    m_stop = m_native.submit(NativeBridge::CaptureStop,
                             QJsonObject{{"duration_ms", 10000},
                                         {"sample_rate", 8000},
                                         {"channels", 1},
                                         {"sample_format", "s16le"}},
                             generation);
    if (!m_stop) {
        cancel();
        emit captured(generation, {}, tr("音频采集服务已关闭"));
    }
}
void NativeRecognitionCapture::cancel()
{
    ++m_generation;
    m_start = m_stop = 0;
    if (m_owned)
        m_native.submit(NativeBridge::CaptureCancel, QJsonObject{}, m_generation);
    m_owned = false;
}
} // namespace EchoMusic
