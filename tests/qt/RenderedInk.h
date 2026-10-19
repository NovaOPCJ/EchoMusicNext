#pragma once
#include <QColor>
#include <QImage>
#include <QMarginsF>
#include <QQuickItem>
#include <QQuickWindow>
#include <optional>

namespace EchoMusic::Test
{
inline std::optional<QRect> renderedInkBounds(const QImage &image, const QRect &region,
                                              const QColor &foreground)
{
    int left = image.width(), top = image.height(), right = -1, bottom = -1;
    const auto pixels = region.intersected(image.rect());
    for (int y = pixels.top(); y <= pixels.bottom(); ++y)
        for (int x = pixels.left(); x <= pixels.right(); ++x) {
            const auto color = image.pixelColor(x, y);
            if (qAbs(color.red() - foreground.red()) + qAbs(color.green() - foreground.green()) +
                    qAbs(color.blue() - foreground.blue()) <
                60) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    return right < left ? std::nullopt
                        : std::optional<QRect>(QRect(QPoint(left, top), QPoint(right, bottom)));
}
// Measure the painted foreground, independently of QML's font/item boxes.
inline std::optional<qreal> renderedInkCenter(const QImage &image, QQuickWindow *window,
                                              QQuickItem *item, const QColor &foreground,
                                              const QMarginsF &inset = {})
{
    const qreal ratio = qreal(image.width()) / window->width();
    const auto scene = item->mapRectToScene(item->boundingRect().marginsRemoved(inset));
    const auto pixels = QRectF(scene.topLeft() * ratio, scene.size() * ratio)
                            .toAlignedRect()
                            .intersected(image.rect());
    int top = image.height(), bottom = -1;
    for (int y = pixels.top(); y <= pixels.bottom(); ++y)
        for (int x = pixels.left(); x <= pixels.right(); ++x) {
            const auto color = image.pixelColor(x, y);
            if (qAbs(color.red() - foreground.red()) + qAbs(color.green() - foreground.green()) +
                    qAbs(color.blue() - foreground.blue()) <
                60) {
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    return bottom < top ? std::nullopt : std::optional<qreal>((top + bottom + 1) / (2 * ratio));
}
} // namespace EchoMusic::Test
