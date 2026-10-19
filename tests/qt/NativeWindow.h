#pragma once
#include <QCursor>
#include <QGuiApplication>
#include <QWindow>
#include <QtTest>

namespace EchoMusic::Test
{
/// A CLI test can be a background process. On Cocoa requestActivate() makes a
/// window key but does not foreground the process; raise() supplies that Qt
/// platform step. Keep native input preconditions shared across formal UI tests.
inline bool prepareNativeInput(QWindow *window)
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        return true;
    if (!QTest::qWaitForWindowExposed(window))
        return false;
    window->raise();
    window->requestActivate();
    return QTest::qWaitForWindowActive(window);
}
/// QTest's QWindow mouse events do not move the platform cursor. Cocoa can
/// emit a later enter/leave from that cursor when popups or render captures
/// update the window. Keep the real pointer and injected events consistent.
inline void moveNativePointer(QWindow *window, QPoint position)
{
    if (QGuiApplication::platformName() != QStringLiteral("offscreen"))
        QCursor::setPos(window->mapToGlobal(position));
    QTest::mouseMove(window, position);
}
} // namespace EchoMusic::Test
