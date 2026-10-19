#include "AccentPalette.h"
#include "ThemeStore.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace EchoMusic
{
namespace
{
struct Lab
{
    double l, a, b;
};
double linear(double v)
{
    return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
}
double gamma(double v)
{
    return v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1.0 / 2.4) - 0.055;
}
Lab toLab(const QColor &rgb)
{
    // QColor::redF() returns float in Qt 6. Use exact 8-bit channels and double
    // precision for stable binary-search rounding near neutral hues.
    const auto r = linear(rgb.red() / 255.0), g = linear(rgb.green() / 255.0),
               b = linear(rgb.blue() / 255.0);
    const auto l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const auto m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const auto s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s};
}
std::array<double, 3> channels(Lab lab)
{
    const auto x = std::pow(lab.l + 0.3963377774 * lab.a + 0.2158037573 * lab.b, 3);
    const auto y = std::pow(lab.l - 0.1055613458 * lab.a - 0.0638541728 * lab.b, 3);
    const auto z = std::pow(lab.l - 0.0894841775 * lab.a - 1.291485548 * lab.b, 3);
    return {4.0767416621 * x - 3.3077115913 * y + 0.2309699292 * z,
            -1.2684380046 * x + 2.6097574011 * y - 0.3413193965 * z,
            -0.0041960863 * x - 0.7034186147 * y + 1.707614701 * z};
}
bool inGamut(const std::array<double, 3> &rgb)
{
    return std::all_of(rgb.begin(), rgb.end(),
                       [](double c) { return c >= -1e-7 && c <= 1 + 1e-7; });
}
QColor fromLab(Lab lab)
{
    lab.l = std::clamp(lab.l, 0.0, 1.0);
    auto rgb = channels(lab);
    if (!inGamut(rgb)) {
        // Preserve hue/lightness by reducing chroma, rather than clipping RGB.
        double low = 0, high = 1;
        for (int i = 0; i < 22; ++i) {
            const auto scale = (low + high) / 2;
            if (inGamut(channels({lab.l, lab.a * scale, lab.b * scale})))
                low = scale;
            else
                high = scale;
        }
        rgb = channels({lab.l, lab.a * low, lab.b * low});
    }
    return QColor(qRound(std::clamp(gamma(rgb[0]), 0.0, 1.0) * 255),
                  qRound(std::clamp(gamma(rgb[1]), 0.0, 1.0) * 255),
                  qRound(std::clamp(gamma(rgb[2]), 0.0, 1.0) * 255));
}
QColor readable(const QColor &color, const QList<QColor> &surfaces, bool dark)
{
    const auto passes = [&surfaces](const QColor &value) {
        return std::all_of(surfaces.begin(), surfaces.end(), [&value](const QColor &bg) {
            return ThemeStore::contrast(value, bg) >= 4.55;
        });
    };
    if (passes(color))
        return color;
    double target = dark ? 1 : 0;
    if (!passes(target ? Qt::white : Qt::black)) {
        target = 1 - target;
        if (!passes(target ? Qt::white : Qt::black))
            return color;
    }
    const auto lab = toLab(color);
    double low = 0, high = 1;
    for (int i = 0; i < 22; ++i) {
        const auto t = (low + high) / 2;
        if (passes(fromLab({lab.l + (target - lab.l) * t, lab.a, lab.b})))
            high = t;
        else
            low = t;
    }
    return fromLab({lab.l + (target - lab.l) * high, lab.a, lab.b});
}
} // namespace
QColor normalizeAccent(const QColor &color, bool dark)
{
    if (!color.isValid() || color.alpha() != 255)
        return QColor("#0071e3");
    const double r = color.red() / 255.0, g = color.green() / 255.0, b = color.blue() / 255.0;
    const auto maximum = std::max({r, g, b}), minimum = std::min({r, g, b}),
               delta = maximum - minimum;
    const auto lightness = (maximum + minimum) / 2;
    const auto saturation = delta == 0 ? 0 : delta / (1 - std::abs(2 * lightness - 1));
    if (saturation < 0.08)
        return QColor("#0071e3");
    double hue = maximum == r   ? std::fmod((g - b) / delta, 6)
                 : maximum == g ? (b - r) / delta + 2
                                : (r - g) / delta + 4;
    hue *= 60;
    if (hue < 0)
        hue += 360;
    // Avoid QColor's quantized HSL conversion, which can change one hex channel
    // when rounding near a clamp edge.
    const auto sat = std::clamp(saturation, dark ? 0.45 : 0.55, dark ? 0.85 : 0.9);
    const auto lit = std::clamp(lightness, dark ? 0.55 : 0.42, dark ? 0.72 : 0.55);
    const auto c = (1 - std::abs(2 * lit - 1)) * sat;
    hue = std::fmod(std::fmod(hue, 360) + 360, 360);
    const auto x = c * (1 - std::abs(std::fmod(hue / 60, 2) - 1)), m = lit - c / 2;
    std::array<double, 3> rgb;
    if (hue < 60)
        rgb = {c, x, 0};
    else if (hue < 120)
        rgb = {x, c, 0};
    else if (hue < 180)
        rgb = {0, c, x};
    else if (hue < 240)
        rgb = {0, x, c};
    else if (hue < 300)
        rgb = {x, 0, c};
    else
        rgb = {c, 0, x};
    return QColor(qRound((rgb[0] + m) * 255), qRound((rgb[1] + m) * 255),
                  qRound((rgb[2] + m) * 255));
}
AccentPalette accentPalette(const QColor &primary, bool dark, const QList<QColor> &surfaces)
{
    const auto lab = toLab(primary);
    const auto chroma = std::hypot(lab.a, lab.b);
    const auto scale = chroma > 0 ? std::min(0.55, 0.065 / chroma) : 0;
    const QColor on =
        ThemeStore::contrast(primary, Qt::white) >= ThemeStore::contrast(primary, Qt::black)
            ? Qt::white
            : Qt::black;
    const double direction = on == Qt::white ? -1 : 1;
    const auto background =
        surfaces.isEmpty() ? QColor(dark ? "#36363a" : "#f5f5f7") : surfaces.first();
    const auto alpha = dark ? 0.18 : 0.12;
    const QColor subtle(qRound(primary.red() * alpha + background.red() * (1 - alpha)),
                        qRound(primary.green() * alpha + background.green() * (1 - alpha)),
                        qRound(primary.blue() * alpha + background.blue() * (1 - alpha)));
    auto backgrounds = surfaces;
    backgrounds.append(subtle);
    return {primary,
            fromLab({dark ? 0.56 : 0.88, lab.a * scale, lab.b * scale}),
            readable(primary, backgrounds, dark),
            on,
            fromLab({lab.l + direction * 0.035, lab.a, lab.b}),
            fromLab({lab.l + direction * 0.065, lab.a, lab.b}),
            subtle,
            readable(primary, {subtle}, dark)};
}
} // namespace EchoMusic
