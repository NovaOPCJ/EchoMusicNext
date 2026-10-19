#include "SettingsMarkdownRenderer.h"
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QUrl>
namespace EchoMusic
{
QString SettingsMarkdownRenderer::render(const QString &markdown, const QFont &font,
                                         const QColor &text, const QColor &secondary,
                                         const QColor &accent) const
{
    if (markdown.size() > 1024 * 1024)
        return tr("更新日志超过大小限制");
    QTextDocument document;
    document.setUndoRedoEnabled(false);
    document.setDefaultFont(font);
    document.setDocumentMargin(0);
    document.setMarkdown(markdown,
                         QTextDocument::MarkdownFeatures{QTextDocument::MarkdownDialectGitHub,
                                                         QTextDocument::MarkdownNoHTML});
    QList<QPair<int, int>> images;
    struct Link
    {
        int position, length;
        QTextCharFormat format;
    };
    QList<Link> links;
    for (auto block = document.begin(); block.isValid(); block = block.next()) {
        auto blockFormat = block.blockFormat();
        const auto heading = blockFormat.headingLevel();
        blockFormat.setTopMargin(heading ? (block == document.begin() ? 0
                                            : heading <= 2            ? 16
                                                                      : 12)
                                         : 4);
        blockFormat.setBottomMargin(heading == 1 ? 8 : heading == 2 ? 6 : 4);
        blockFormat.setLineHeight(block.textList() ? 28 : 162.5,
                                  block.textList() ? QTextBlockFormat::FixedHeight
                                                   : QTextBlockFormat::ProportionalHeight);
        QTextCursor cursor(block);
        cursor.setBlockFormat(blockFormat);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        QTextCharFormat common;
        common.setForeground(heading ? text : secondary);
        if (heading) {
            auto headingFont = font;
            headingFont.setPixelSize(heading == 1 ? 16 : 14);
            headingFont.setWeight(heading <= 2 ? QFont::Bold : QFont::DemiBold);
            common.setFont(headingFont);
            common.setProperty(QTextFormat::FontSizeAdjustment, 0);
        }
        cursor.mergeCharFormat(common);
        for (auto fragment = block.begin(); !fragment.atEnd(); ++fragment) {
            const auto part = fragment.fragment();
            const auto format = part.charFormat();
            if (format.isImageFormat()) {
                images.append({part.position(), part.length()});
                continue;
            }
            if (!format.isAnchor())
                continue;
            auto link = format;
            const QUrl url(format.anchorHref());
            if (url.scheme() == "https" && !url.host().isEmpty() && url.userInfo().isEmpty()) {
                link.setForeground(accent);
                link.setFontUnderline(false);
            } else {
                link.setAnchor(false);
                link.setAnchorHref({});
                link.setForeground(secondary);
            }
            links.append({part.position(), part.length(), link});
        }
    }
    // Apply after traversal: changing a fragment's format may merge it with a
    // neighbor and invalidate QTextBlock::iterator.
    for (const auto &link : links) {
        QTextCursor cursor(&document);
        cursor.setPosition(link.position);
        cursor.setPosition(link.position + link.length, QTextCursor::KeepAnchor);
        cursor.setCharFormat(link.format);
    }
    // QQuick rich text may resolve image resources. Notes are text-only: never
    // let opening a settings dialog trigger remote image requests or local reads.
    for (auto it = images.crbegin(); it != images.crend(); ++it) {
        QTextCursor cursor(&document);
        cursor.setPosition(it->first);
        cursor.setPosition(it->first + it->second, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
    return document.toHtml();
}
} // namespace EchoMusic
