#pragma once
#include <QString>
#include <QFont>

namespace EchoMusic
{
/// Install the platform system-ui font before creating any QML engine. Controls
/// and standalone Text share the same baseline and retain glyph fallback. macOS
/// explicitly falls back to PingFang SC to select real CJK weight variants.
void configureUiTypography(const QString &family = QStringLiteral("system-ui"));

/// Resolve an independent system/user family with the same platform fallback.
QFont uiFont(const QString &family);

/// Copy the full font chain without per-delegate database queries.
QFont styledUiFont(const QFont &base, qreal pixelSize, int weight, qreal letterSpacing, bool italic,
                   bool underline);
} // namespace EchoMusic
