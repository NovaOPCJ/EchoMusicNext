#include "LyricAppearance.h"
#include <QColor>
#include <cmath>
namespace EchoMusic
{
LyricAppearance::LyricAppearance(QSettings &settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
    setPlayedColor(settings.value("lyrics/page/playedColor", "").toString());
    setUnplayedColor(settings.value("lyrics/page/unplayedColor", "").toString());
    setFollowCoverColor(settings.value("lyrics/page/followCoverColor", false).toBool());
    setUnplayedFollowCoverColor(
        settings.value("lyrics/page/unplayedFollowCoverColor", false).toBool());
    setAlignPosition(settings.value("lyrics/appearance/alignPosition", 0.5).toDouble());
    setEnableSpring(settings.value("lyrics/appearance/enableSpring", true).toBool());
    setHighRefreshRate(settings.value("lyrics/appearance/highRefreshRate", false).toBool());
    setEnableBlur(settings.value("lyrics/appearance/enableBlur", true).toBool());
    setEnableScale(settings.value("lyrics/appearance/enableScale", true).toBool());
    setHidePassedLines(settings.value("lyrics/appearance/hidePassedLines", false).toBool());
    setWordFadeWidth(settings.value("lyrics/appearance/wordFadeWidth", 0.5).toDouble());
    setDynamicAlbumCover(settings.value("lyrics/appearance/dynamicAlbumCover", false).toBool());
    setTextColor(settings.value("lyrics/appearance/textColor", QString()).toString());
    setUseCoverColor(settings.value("lyrics/appearance/useCoverColor", false).toBool());
    setFontScale(settings.value("lyrics/appearance/fontScale", 1.).toDouble());
    setFontWeight(settings.value("lyrics/appearance/fontWeight", 900).toInt());
    setBackgroundBlur(settings.value("lyrics/appearance/backgroundBlur", false).toBool());
    setBackgroundRhythm(settings.value("lyrics/appearance/backgroundRhythm", false).toBool());
}
void LyricAppearance::setPlayedColor(const QString &value)
{
    if (!value.isEmpty() && !QColor(value).isValid())
        return;
    if (m_playedColor == value)
        return;
    m_playedColor = value;
    m_settings.setValue("lyrics/page/playedColor", value);
    emit changed();
}
void LyricAppearance::setUnplayedColor(const QString &value)
{
    if (!value.isEmpty() && !QColor(value).isValid())
        return;
    if (m_unplayedColor == value)
        return;
    m_unplayedColor = value;
    m_settings.setValue("lyrics/page/unplayedColor", value);
    emit changed();
}
void LyricAppearance::setFollowCoverColor(bool value)
{
    if (m_followCoverColor == value)
        return;
    m_followCoverColor = value;
    m_settings.setValue("lyrics/page/followCoverColor", value);
    emit changed();
}
void LyricAppearance::setUnplayedFollowCoverColor(bool value)
{
    if (m_unplayedFollowCoverColor == value)
        return;
    m_unplayedFollowCoverColor = value;
    m_settings.setValue("lyrics/page/unplayedFollowCoverColor", value);
    emit changed();
}
void LyricAppearance::resetColors()
{
    setPlayedColor("");
    setUnplayedColor("");
    setFollowCoverColor(false);
    setUnplayedFollowCoverColor(false);
}
bool LyricAppearance::customized() const
{
    return m_alignPosition != 0.5 || m_enableSpring != true || m_highRefreshRate != false ||
           m_enableBlur != true || m_enableScale != true || m_hidePassedLines != false ||
           m_wordFadeWidth != 0.5 || m_dynamicAlbumCover != false || m_textColor != QString() ||
           m_useCoverColor != false || m_fontScale != 1. || m_fontWeight != 900;
}
void LyricAppearance::resetSkin()
{
    setAlignPosition(0.5);
    setEnableSpring(true);
    setHighRefreshRate(false);
    setEnableBlur(true);
    setEnableScale(true);
    setHidePassedLines(false);
    setWordFadeWidth(0.5);
    setDynamicAlbumCover(false);
    setTextColor(QString());
    setUseCoverColor(false);
    setFontScale(1.);
    setFontWeight(900);
}
void LyricAppearance::setAlignPosition(double value)
{
    if (!std::isfinite(value))
        return;
    const auto normalized = qBound(0., value, 1.);
    if (m_alignPosition == normalized)
        return;
    m_alignPosition = normalized;
    m_settings.setValue("lyrics/appearance/alignPosition", normalized);
    emit changed();
}
void LyricAppearance::setEnableSpring(bool value)
{
    const auto normalized = value;
    if (m_enableSpring == normalized)
        return;
    m_enableSpring = normalized;
    m_settings.setValue("lyrics/appearance/enableSpring", normalized);
    emit changed();
}
void LyricAppearance::setHighRefreshRate(bool value)
{
    const auto normalized = value;
    if (m_highRefreshRate == normalized)
        return;
    m_highRefreshRate = normalized;
    m_settings.setValue("lyrics/appearance/highRefreshRate", normalized);
    emit changed();
}
void LyricAppearance::setEnableBlur(bool value)
{
    const auto normalized = value;
    if (m_enableBlur == normalized)
        return;
    m_enableBlur = normalized;
    m_settings.setValue("lyrics/appearance/enableBlur", normalized);
    emit changed();
}
void LyricAppearance::setEnableScale(bool value)
{
    const auto normalized = value;
    if (m_enableScale == normalized)
        return;
    m_enableScale = normalized;
    m_settings.setValue("lyrics/appearance/enableScale", normalized);
    emit changed();
}
void LyricAppearance::setHidePassedLines(bool value)
{
    const auto normalized = value;
    if (m_hidePassedLines == normalized)
        return;
    m_hidePassedLines = normalized;
    m_settings.setValue("lyrics/appearance/hidePassedLines", normalized);
    emit changed();
}
void LyricAppearance::setWordFadeWidth(double value)
{
    if (!std::isfinite(value))
        return;
    const auto normalized = qBound(.05, value, 1.);
    if (m_wordFadeWidth == normalized)
        return;
    m_wordFadeWidth = normalized;
    m_settings.setValue("lyrics/appearance/wordFadeWidth", normalized);
    emit changed();
}
void LyricAppearance::setDynamicAlbumCover(bool value)
{
    const auto normalized = value;
    if (m_dynamicAlbumCover == normalized)
        return;
    m_dynamicAlbumCover = normalized;
    m_settings.setValue("lyrics/appearance/dynamicAlbumCover", normalized);
    emit changed();
}
void LyricAppearance::setTextColor(const QString &value)
{
    if (!value.isEmpty() && !QColor(value).isValid())
        return;
    const auto normalized = value;
    if (m_textColor == normalized)
        return;
    m_textColor = normalized;
    m_settings.setValue("lyrics/appearance/textColor", normalized);
    emit changed();
}
void LyricAppearance::setUseCoverColor(bool value)
{
    const auto normalized = value;
    if (m_useCoverColor == normalized)
        return;
    m_useCoverColor = normalized;
    m_settings.setValue("lyrics/appearance/useCoverColor", normalized);
    emit changed();
}
void LyricAppearance::setFontScale(double value)
{
    if (!std::isfinite(value))
        return;
    const auto normalized = qBound(.7, value, 1.4);
    if (m_fontScale == normalized)
        return;
    m_fontScale = normalized;
    m_settings.setValue("lyrics/appearance/fontScale", normalized);
    emit changed();
}
void LyricAppearance::setFontWeight(int value)
{
    const auto normalized = qBound(100, qRound(value / 100.) * 100, 900);
    if (m_fontWeight == normalized)
        return;
    m_fontWeight = normalized;
    m_settings.setValue("lyrics/appearance/fontWeight", normalized);
    emit changed();
}
void LyricAppearance::setBackgroundBlur(bool value)
{
    const auto normalized = value;
    if (m_backgroundBlur == normalized)
        return;
    m_backgroundBlur = normalized;
    m_settings.setValue("lyrics/appearance/backgroundBlur", normalized);
    emit changed();
}
void LyricAppearance::setBackgroundRhythm(bool value)
{
    const auto normalized = value;
    if (m_backgroundRhythm == normalized)
        return;
    m_backgroundRhythm = normalized;
    m_settings.setValue("lyrics/appearance/backgroundRhythm", normalized);
    emit changed();
}
} // namespace EchoMusic
