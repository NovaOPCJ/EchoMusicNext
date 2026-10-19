#pragma once
#include <QColor>
#include <QFont>
#include <QObject>
#include <QtQml/qqmlregistration.h>
namespace EchoMusic
{
/// Bounded Qt rich-text formatting for the two release-note surfaces. This
/// object owns no preference/font state; callers supply the complete Theme font.
class SettingsMarkdownRenderer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    using QObject::QObject;
    Q_INVOKABLE QString render(const QString &markdown, const QFont &font, const QColor &text,
                               const QColor &secondary, const QColor &accent) const;
};
} // namespace EchoMusic
