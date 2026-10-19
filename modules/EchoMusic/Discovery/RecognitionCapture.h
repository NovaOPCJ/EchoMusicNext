#pragma once
#include "NativeBridge.h"
#include <QObject>
namespace EchoMusic
{
/// Typed capture boundary; test implementations exercise the same service state
/// machine without requesting access to a user's real microphone.
class RecognitionCapture : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void start(quint64 generation, const QString &source, const QString &device) = 0;
    virtual void stop(quint64 generation) = 0;
    virtual void cancel() = 0;
signals:
    void started(quint64 generation, const QString &error);
    void captured(quint64 generation, const QByteArray &pcm, const QString &error);
};
class NativeRecognitionCapture final : public RecognitionCapture
{
    Q_OBJECT
public:
    explicit NativeRecognitionCapture(NativeBridge &, QObject *parent = nullptr);
    ~NativeRecognitionCapture() override;
    void start(quint64, const QString &, const QString &) override;
    void stop(quint64) override;
    void cancel() override;

private:
    void begin(quint64, const QString &, const QString &);
    NativeBridge &m_native;
    quint64 m_generation = 0, m_start = 0, m_stop = 0;
    bool m_owned = false;
};
} // namespace EchoMusic
