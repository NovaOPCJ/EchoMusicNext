#import <Cocoa/Cocoa.h>
#include "WindowController.h"
#include <QMenu>
#include <functional>

namespace EchoMusic
{
class DockMenuOwner;
}

// Preserve Qt's application delegate; only Dock requests and our own commands
// are handled here. The target outlives each menu exported to the Dock process.
@interface EchoDockMenuDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, assign) EchoMusic::DockMenuOwner *owner;
@property(nonatomic, retain) id previous;
@end

namespace EchoMusic
{
namespace
{
NSMenu *snapshot(QMenu *source, id target)
{
    auto *native = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    native.autoenablesItems = NO;
    for (auto *action : source->actions()) {
        if (action->isSeparator()) {
            [native addItem:NSMenuItem.separatorItem];
            continue;
        }
        auto *item = [[[NSMenuItem alloc] initWithTitle:action->text().toNSString()
                                                 action:nil
                                          keyEquivalent:@""] autorelease];
        item.enabled = action->isEnabled();
        item.state = action->isChecked() ? NSControlStateValueOn : NSControlStateValueOff;
        if (auto *submenu = action->menu()) {
            item.submenu = snapshot(submenu, target);
        } else if (!action->objectName().isEmpty()) {
            item.target = target;
            item.action = NSSelectorFromString((action->objectName() + ':').toNSString());
        }
        [native addItem:item];
    }
    return native;
}
} // namespace
class DockMenuOwner final : public QObject
{
public:
    DockMenuOwner(WindowController *controller, std::function<QMenu *()> create)
        : m_controller(controller), m_create(std::move(create))
    {
        m_delegate = [[EchoDockMenuDelegate alloc] init];
        m_delegate.owner = this;
        m_delegate.previous = NSApp.delegate;
        NSApp.delegate = m_delegate;
    }
    ~DockMenuOwner() override
    {
        if (NSApp.delegate == m_delegate)
            NSApp.delegate = m_delegate.previous;
        m_delegate.owner = nullptr;
        [m_delegate release];
    }
    NSMenu *open()
    {
        const std::unique_ptr<QMenu> recipe(m_create());
        if (!recipe)
            return nil;
        emit m_controller->nativeMenuOpening();
        // The Dock displays its own serialized copy and does not track our
        // NSMenu locally. Export a snapshot with stable command targets, then
        // release the temporary Qt actions and the autoreleased native recipe.
        return snapshot(recipe.get(), m_delegate);
    }
    void trigger(const QString &command)
    {
        // Re-read current service state/availability at execution time. Commands
        // share the tray's QAction connections without retaining a popup tree.
        const std::unique_ptr<QMenu> recipe(m_create());
        if (!recipe)
            return;
        if (auto *action = recipe->findChild<QAction *>(command); action && action->isEnabled())
            action->trigger();
    }

private:
    WindowController *m_controller;
    std::function<QMenu *()> m_create;
    EchoDockMenuDelegate *m_delegate;
};

QObject *createMacDockMenuOwner(WindowController *controller, std::function<QMenu *()> create)
{
    return new DockMenuOwner(controller, std::move(create));
}
} // namespace EchoMusic

@implementation EchoDockMenuDelegate
- (BOOL)respondsToSelector:(SEL)selector
{
    return [super respondsToSelector:selector] || [self.previous respondsToSelector:selector];
}
- (id)forwardingTargetForSelector:(SEL)selector
{
    return [self.previous respondsToSelector:selector]
               ? self.previous
               : [super forwardingTargetForSelector:selector];
}
- (NSMenu *)applicationDockMenu:(NSApplication *)sender
{
    Q_UNUSED(sender);
    return self.owner ? self.owner->open() : nil;
}
- (void)run:(SEL)command
{
    if (self.owner)
        self.owner->trigger(QString::fromNSString(NSStringFromSelector(command)).chopped(1));
}
- (void)togglePlayback:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)previous:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)next:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)sequential:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)listLoop:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)shuffle:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)singleLoop:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)volumeUp:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)volumeDown:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)toggleMute:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)toggleLyricsLock:(id)sender
{
    Q_UNUSED(sender);
    [self run:_cmd];
}
- (void)dealloc
{
    self.previous = nil;
    [super dealloc];
}
@end
