#include "CommentTextStyle.h"
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
namespace EchoMusic
{
void CommentTextStyle::setLineHeight(qreal height)
{
    if (!qIsFinite(height) || height <= 0 || qFuzzyCompare(height, m_lineHeight))
        return;
    m_lineHeight = height;
    apply();
    emit lineHeightChanged();
}
void CommentTextStyle::setDocument(QQuickTextDocument *d)
{
    if (d == m_document)
        return;
    disconnect(m_documentChanged);
    disconnect(m_contents);
    m_document = d;
    if (d)
        m_documentChanged =
            connect(d, &QQuickTextDocument::textDocumentChanged, this, &CommentTextStyle::attach);
    attach();
    emit documentChanged();
}
void CommentTextStyle::attach()
{
    disconnect(m_contents);
    m_text = m_document ? m_document->textDocument() : nullptr;
    if (m_text)
        m_contents =
            connect(m_text, &QTextDocument::contentsChanged, this, &CommentTextStyle::apply);
    apply();
}
void CommentTextStyle::apply()
{
    if (!m_text || m_applying)
        return;
    m_applying = true;
    for (auto block = m_text->begin(); block.isValid(); block = block.next()) {
        auto format = block.blockFormat();
        if (format.lineHeightType() == QTextBlockFormat::FixedHeight &&
            qFuzzyCompare(format.lineHeight(), m_lineHeight))
            continue;
        // CSS 1.75 is relative to pixelSize, whereas QTextBlockFormat's
        // proportional height multiplies the font's larger native line box.
        format.setLineHeight(m_lineHeight, QTextBlockFormat::FixedHeight);
        QTextCursor cursor(block);
        cursor.setBlockFormat(format);
    }
    m_applying = false;
}
} // namespace EchoMusic
