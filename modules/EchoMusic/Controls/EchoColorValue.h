#pragma once
#include <QObject>
#include <QColor>
#include <QFont>
#include <QtQml/qqmlregistration.h>
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
    explicit EchoColorValue(QObject *parent = nullptr) : QObject(parent) {}
    QFont fixedFont() const;
    Q_INVOKABLE QString normalize(const QString &text) const;
    Q_INVOKABLE qreal contrast(const QColor &a, const QColor &b) const;
    Q_INVOKABLE QVariantMap hsv(const QString &text) const;
    Q_INVOKABLE QString fromHsv(qreal hue, qreal saturation, qreal value) const;
};
} // namespace EchoMusic
