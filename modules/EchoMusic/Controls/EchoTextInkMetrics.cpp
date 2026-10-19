#include "EchoTextInkMetrics.h"
#include <QFontMetricsF>
#include <QPainterPath>
#include <QTextLayout>
#include <algorithm>

namespace EchoMusic
{
void EchoTextInkMetrics::setFont(const QFont &font)
{
    if (font_ == font)
        return;
    font_ = font;
    outlineText_.clear();
    outlineBounds_ = {};
    measure();
    emit fontChanged();
}
void EchoTextInkMetrics::setText(const QString &text)
{
    if (text_ == text)
        return;
    text_ = text;
    measure();
    emit textChanged();
}
void EchoTextInkMetrics::setLayoutWidth(qreal width)
{
    width = std::max(qreal(0), width);
    if (width_ == width)
        return;
    width_ = width;
    measure();
    emit layoutWidthChanged();
}
void EchoTextInkMetrics::setLineHeight(qreal height)
{
    height = std::max(qreal(0), height);
    if (lineHeight_ == height)
        return;
    lineHeight_ = height;
    measure();
    emit lineHeightChanged();
}
void EchoTextInkMetrics::setWrap(bool wrap)
{
    if (wrap_ == wrap)
        return;
    wrap_ = wrap;
    measure();
    emit wrapChanged();
}
void EchoTextInkMetrics::setElide(bool elide)
{
    if (elide_ == elide)
        return;
    elide_ = elide;
    measure();
    emit elideChanged();
}
void EchoTextInkMetrics::measure()
{
    const QFontMetricsF fm(font_);
    qreal naturalWidth = 0;
    for (const auto &line : text_.split(u'\n'))
        naturalWidth = std::max(naturalWidth, fm.horizontalAdvance(line));
    if (naturalWidth_ != naturalWidth) {
        naturalWidth_ = naturalWidth;
        emit naturalWidthChanged();
    }
    displayText_ = elide_ && !wrap_ ? fm.elidedText(text_, Qt::ElideRight, width_) : text_;
    QTextOption option;
    option.setUseDesignMetrics(true); // Qt Quick's default distance-field Text layout.
    option.setWrapMode(wrap_ ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    if (!wrap_ && !displayText_.contains(u'\n')) {
        auto bounds = fm.tightBoundingRect(displayText_, option);
        // Native font metrics can include a descender pixel absent from the
        // visible Latin/digit outline. Qt shapes this entire line; no custom
        // glyph walk or per-entry correction. Color/fallback text keeps its
        // existing metrics (including emoji with no vector outline).
        const bool outlineLine =
            std::all_of(displayText_.cbegin(), displayText_.cend(), [](QChar c) {
                return (c.unicode() >= 32 && c.unicode() < 127) || c == QChar(0x2026);
            });
        if (outlineLine) {
            // Cache only the last measured string/bounds. Unchanged text on
            // resize does not create paths or repeat QTextLayout; temporary
            // outline data is released immediately after measuring.
            if (outlineText_ != displayText_) {
                QPainterPath outline;
                outline.addText(QPointF(), font_, displayText_);
                outlineText_ = displayText_;
                outlineBounds_ = outline.boundingRect();
            }
            if (!outlineBounds_.isEmpty())
                bounds = outlineBounds_;
        }
        hasInk_ = !bounds.isEmpty();
        inkCenter_ = hasInk_ ? bounds.center().y() : 0;
        firstLineInkCenter_ = inkCenter_;
        emit metricsChanged();
        return;
    }
    QTextLayout layout(QString(displayText_).replace(u'\n', QChar::LineSeparator), font_);
    layout.setTextOption(option);
    layout.beginLayout();
    qreal y = 0, firstBaseline = 0;
    QRectF ink;
    bool first = true;
    firstLineInkCenter_ = 0;
    while (true) {
        auto line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(wrap_ ? std::max(qreal(1), width_) : 1e9);
        line.setPosition({0, y});
        // Qt's native tight bounds include shaping and font fallback. Only
        // wrapping/line spacing needs QTextLayout; do not walk individual glyphs
        // or reconstruct their outlines to implement ordinary UI alignment.
        const auto lineText = layout.text().mid(line.textStart(), line.textLength());
        const auto bounds = fm.tightBoundingRect(lineText, option);
        if (first) {
            firstBaseline = line.ascent();
            firstLineInkCenter_ = bounds.isEmpty() ? 0 : bounds.center().y();
            first = false;
        }
        if (!bounds.isEmpty())
            ink = ink.united(bounds.translated(0, line.y() + line.ascent()));
        y += lineHeight_ > 0 ? lineHeight_ : line.height();
    }
    layout.endLayout();
    hasInk_ = !ink.isEmpty();
    inkCenter_ = hasInk_ ? ink.center().y() - firstBaseline : 0;
    emit metricsChanged();
}
} // namespace EchoMusic
