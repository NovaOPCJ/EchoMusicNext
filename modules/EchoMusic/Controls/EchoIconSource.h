#pragma once
#include <QColor>
#include <QObject>
#include <QQuickImageProvider>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
namespace EchoMusic
{
/// Stateless, reentrant rasterization of bundled monochrome glyphs. The QML
/// image cache shares results by glyph/color/requested pixel size, not theme.
class EchoIconImageProvider final : public QQuickImageProvider
{
public:
    EchoIconImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

/// Engine-local provider installation, so every window/test/plugin engine uses
/// the same icon contract without application-specific setup or GPU effects.
class EchoIconSource final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    // No default constructor: QML otherwise prefers it over create(), skipping
    // engine-local provider installation.
    explicit EchoIconSource(QObject *parent) : QObject(parent) {}
    static EchoIconSource *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    Q_INVOKABLE QUrl urlFor(const QString &glyph, const QColor &color) const;
};
} // namespace EchoMusic
