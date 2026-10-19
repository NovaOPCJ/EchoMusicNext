#pragma once
#include <QObject>
namespace EchoMusic
{
class ShortcutService;
class PlaybackService;
class ShellSettings;
class WindowController;
class ShellController;
/// Typed command composition shared by local/global keyboard paths. Owners keep
/// their existing state and units; presentation-only popup actions stay in QML.
class ShortcutActions final : public QObject
{
public:
    ShortcutActions(ShortcutService &shortcuts, PlaybackService &playback, ShellSettings &settings,
                    WindowController &window, ShellController &shell, QObject *parent = nullptr);
};
} // namespace EchoMusic
