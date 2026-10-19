#include "EchoPageScrollMetrics.h"
#include <QGuiApplication>
#include <QStyleHints>

namespace EchoMusic
{
EchoPageScrollMetrics::EchoPageScrollMetrics(QObject *parent) : QObject(parent)
{
    // Context ownership disconnects this projection when its QML engine dies.
    connect(QGuiApplication::styleHints(), &QStyleHints::wheelScrollLinesChanged, this,
            &EchoPageScrollMetrics::wheelStepChanged);
}
int EchoPageScrollMetrics::wheelStep() const
{
    // Qt 6.11 Flickable uses 24 DIP per configured wheel line. Pixel-delta
    // touchpads bypass this metric; it is not a second mutable preference.
    return QGuiApplication::styleHints()->wheelScrollLines() * 24;
}
} // namespace EchoMusic
