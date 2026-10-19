#pragma once
#include <QObject>
#include <QColor>
#include <QFont>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
namespace EchoMusic
{
/// Pure presentation helpers for color dialogs; no writable theme preferences.
class EchoColorValue final : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QFont fixedFont READ fixedFont CONSTANT)
public:
    // No default constructor: QML otherwise instantiates through QQmlElement<T>,
    // which cannot derive from a final class.
    explicit EchoColorValue(QObject *parent) : QObject(parent) {}
    static EchoColorValue *create(QQmlEngine *engine, QJSEngine *scriptEngine);
    QFont fixedFont() const;
    Q_INVOKABLE QString normalize(const QString &text) const;
    Q_INVOKABLE qreal contrast(const QColor &a, const QColor &b) const;
    Q_INVOKABLE QVariantMap hsv(const QString &text) const;
    Q_INVOKABLE QString fromHsv(qreal hue, qreal saturation, qreal value) const;
};
} // namespace EchoMusic
