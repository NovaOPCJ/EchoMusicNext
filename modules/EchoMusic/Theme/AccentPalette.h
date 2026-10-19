#pragma once
#include <QColor>
#include <QList>

namespace EchoMusic
{
struct AccentPalette
{
    QColor primary, atmosphere, primaryText, onPrimary, hover, pressed, subtle, onSubtle;
};
/// Theme colors derived from EchoMusic shared/accentPalette.ts, GPL-3.0-only.
/// HSL normalization and OKLab/D65 state colors keep selected and derived colors separate.
QColor normalizeAccent(const QColor &color, bool dark);
AccentPalette accentPalette(const QColor &primary, bool dark, const QList<QColor> &surfaces);
} // namespace EchoMusic
