#pragma once
#include <QColor>
#include <QImage>
#include <QRectF>
#include <QString>
#include <atomic>
#include <memory>

namespace EchoMusic
{
struct ThemeImageResult
{
    QString id, error;
    QSize size;
    QColor sample;
};
/// Worker-only, bounded image normalization. Managed image IDs are content
/// hashes; no user paths or temporary decoder buffers enter preferences.
class ThemeImageStore final
{
public:
    /// Entry point for the separate decoder process; do not call from the GUI.
    static ThemeImageResult normalize(const QString &source, const QString &directory);
    static ThemeImageResult importImage(const QString &source, const QString &directory,
                                        std::shared_ptr<std::atomic_bool> canceled = {});
    static QColor sample(const QString &directory, const QString &id, const QRectF &crop);
    static bool validId(const QString &id);
    static void clean(const QString &directory, const QStringList &retained);
};
} // namespace EchoMusic
