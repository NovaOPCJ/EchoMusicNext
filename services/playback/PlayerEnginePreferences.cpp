#include "PlayerEnginePreferences.h"
#include <cmath>
namespace EchoMusic
{
namespace
{
double number(const QVariant &value, double fallback, double minimum, double maximum)
{
    bool ok = false;
    const auto result = value.toDouble(&ok);
    return ok && std::isfinite(result) ? qBound(minimum, result, maximum) : fallback;
}
} // namespace
PlayerEnginePreferences::PlayerEnginePreferences(QSettings &settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
    m_demuxerReadaheadSecs =
        number(settings.value("playerEngine/demuxerReadaheadSecs", 1), 1, 0, 3600000);
    m_cacheSecs = number(settings.value("playerEngine/cacheSecs", 3600000), 3600000, 0, 3600000);
    m_cachePauseWaitSecs =
        number(settings.value("playerEngine/cachePauseWaitSecs", 1), 1, 0, 3600000);
    m_demuxerMaxMb = number(settings.value("playerEngine/demuxerMaxMb", 150), 150, 0, 4096);
    m_demuxerMaxBackMb = number(settings.value("playerEngine/demuxerMaxBackMb", 50), 50, 0, 4096);
    m_audioBufferSecs = number(settings.value("playerEngine/audioBufferSecs", 0.2), 0.2, 0, 10);
    m_playResumeTimeout = number(settings.value("playerEngine/playResumeTimeout", 5), 5, 0, 30);
    m_playbackStallTimeout =
        number(settings.value("playerEngine/playbackStallTimeout", 8), 8, 0, 60);
    const auto cacheMode = settings.value("playerEngine/cacheMode", "auto").toString();
    if (QStringList{"auto", "yes", "no"}.contains(cacheMode))
        m_cacheMode = cacheMode;
    const auto audioSamplerate = settings.value("playerEngine/audioSamplerate", "auto").toString();
    if (QStringList{"auto", "44100", "48000", "96000", "192000"}.contains(audioSamplerate))
        m_audioSamplerate = audioSamplerate;
    const auto audioChannels = settings.value("playerEngine/audioChannels", "auto-safe").toString();
    if (QStringList{"auto-safe", "auto", "stereo", "mono"}.contains(audioChannels))
        m_audioChannels = audioChannels;
    const auto audioFormat = settings.value("playerEngine/audioFormat", "auto").toString();
    if (QStringList{"auto", "float", "s16", "s32"}.contains(audioFormat))
        m_audioFormat = audioFormat;
    const auto gaplessAudio = settings.value("playerEngine/gaplessAudio", "weak").toString();
    if (QStringList{"weak", "yes", "no"}.contains(gaplessAudio))
        m_gaplessAudio = gaplessAudio;
    m_cachePause = settings.value("playerEngine/cachePause", true).toBool();
    m_attempts = qBound(1, settings.value("playerEngine/playbackStallMaxAttempts", 3).toInt(), 10);
}
void PlayerEnginePreferences::setDemuxerReadaheadSecs(double value)
{
    if (!std::isfinite(value))
        return;
    value = qBound(0., value, 3600000.);
    if (value == m_demuxerReadaheadSecs)
        return;
    m_demuxerReadaheadSecs = value;
    m_settings.setValue("playerEngine/demuxerReadaheadSecs", value);
    emit changed();
}
void PlayerEnginePreferences::setCacheSecs(double value)
{
    if (!std::isfinite(value))
        return;
    value = qBound(0., value, 3600000.);
    if (value == m_cacheSecs)
        return;
    m_cacheSecs = value;
    m_settings.setValue("playerEngine/cacheSecs", value);
    emit changed();
}
void PlayerEnginePreferences::setCachePauseWaitSecs(double value)
{
    if (!std::isfinite(value))
        return;
    value = qBound(0., value, 3600000.);
    if (value == m_cachePauseWaitSecs)
        return;
    m_cachePauseWaitSecs = value;
    m_settings.setValue("playerEngine/cachePauseWaitSecs", value);
    emit changed();
}
void PlayerEnginePreferences::setDemuxerMaxMb(double value)
{
    if (!std::isfinite(value))
        return;
    value = qBound(0., value, 4096.);
    if (value == m_demuxerMaxMb)
        return;
    m_demuxerMaxMb = value;
    m_settings.setValue("playerEngine/demuxerMaxMb", value);
    emit changed();
}
void PlayerEnginePreferences::setDemuxerMaxBackMb(double value)
{
    if (!std::isfinite(value))
        return;
    value = qBound(0., value, 4096.);
    if (value == m_demuxerMaxBackMb)
        return;
    m_demuxerMaxBackMb = value;
    m_settings.setValue("playerEngine/demuxerMaxBackMb", value);
    emit changed();
}
void PlayerEnginePreferences::setAudioBufferSecs(double value)
{
    if (!std::isfinite(value))
        return;
    value = qBound(0., value, 10.);
    if (value == m_audioBufferSecs)
        return;
    m_audioBufferSecs = value;
    m_settings.setValue("playerEngine/audioBufferSecs", value);
    emit changed();
}
void PlayerEnginePreferences::setPlayResumeTimeout(double value)
{
    if (!std::isfinite(value))
        return;
    value = qBound(0., value, 30.);
    if (value == m_playResumeTimeout)
        return;
    m_playResumeTimeout = value;
    m_settings.setValue("playerEngine/playResumeTimeout", value);
    emit changed();
}
void PlayerEnginePreferences::setPlaybackStallTimeout(double value)
{
    if (!std::isfinite(value))
        return;
    value = qBound(0., value, 60.);
    if (value == m_playbackStallTimeout)
        return;
    m_playbackStallTimeout = value;
    m_settings.setValue("playerEngine/playbackStallTimeout", value);
    emit stallTimeoutChanged(value);
    emit changed();
}
void PlayerEnginePreferences::setCacheMode(const QString &value)
{
    if (value == m_cacheMode || !QStringList{"auto", "yes", "no"}.contains(value))
        return;
    m_cacheMode = value;
    m_settings.setValue("playerEngine/cacheMode", value);
    emit changed();
}
void PlayerEnginePreferences::setAudioSamplerate(const QString &value)
{
    if (value == m_audioSamplerate ||
        !QStringList{"auto", "44100", "48000", "96000", "192000"}.contains(value))
        return;
    m_audioSamplerate = value;
    m_settings.setValue("playerEngine/audioSamplerate", value);
    emit changed();
}
void PlayerEnginePreferences::setAudioChannels(const QString &value)
{
    if (value == m_audioChannels ||
        !QStringList{"auto-safe", "auto", "stereo", "mono"}.contains(value))
        return;
    m_audioChannels = value;
    m_settings.setValue("playerEngine/audioChannels", value);
    emit changed();
}
void PlayerEnginePreferences::setAudioFormat(const QString &value)
{
    if (value == m_audioFormat || !QStringList{"auto", "float", "s16", "s32"}.contains(value))
        return;
    m_audioFormat = value;
    m_settings.setValue("playerEngine/audioFormat", value);
    emit changed();
}
void PlayerEnginePreferences::setGaplessAudio(const QString &value)
{
    if (value == m_gaplessAudio || !QStringList{"weak", "yes", "no"}.contains(value))
        return;
    m_gaplessAudio = value;
    m_settings.setValue("playerEngine/gaplessAudio", value);
    emit changed();
}
void PlayerEnginePreferences::setPlaybackStallMaxAttempts(int value)
{
    value = qBound(1, value, 10);
    if (value == m_attempts)
        return;
    m_attempts = value;
    m_settings.setValue("playerEngine/playbackStallMaxAttempts", value);
    emit changed();
}
void PlayerEnginePreferences::setCachePause(bool enabled)
{
    if (enabled == m_cachePause)
        return;
    m_cachePause = enabled;
    m_settings.setValue("playerEngine/cachePause", enabled);
    emit changed();
}
QJsonObject PlayerEnginePreferences::startupOptions() const
{
    return {{"audio_buffer_secs", m_audioBufferSecs},
            {"audio_samplerate", m_audioSamplerate},
            {"audio_channels", m_audioChannels},
            {"audio_format", m_audioFormat},
            {"gapless_audio", m_gaplessAudio},
            {"demuxer_readahead_secs", m_demuxerReadaheadSecs},
            {"cache", m_cacheMode},
            {"cache_pause", m_cachePause},
            {"cache_secs", m_cacheSecs},
            {"cache_pause_wait_secs", m_cachePauseWaitSecs},
            {"demuxer_max_bytes", m_demuxerMaxMb * 1024 * 1024},
            {"demuxer_max_back_bytes", m_demuxerMaxBackMb * 1024 * 1024},
            {"playback_stall_timeout_secs", m_playbackStallTimeout}};
}
} // namespace EchoMusic
