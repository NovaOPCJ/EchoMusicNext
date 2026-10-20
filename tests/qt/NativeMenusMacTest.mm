#import <Cocoa/Cocoa.h>
#include <QMenu>

namespace
{
NSMenuItem *findItem(NSMenu *menu, const QString &text)
{
    for (NSMenuItem *item in menu.itemArray) {
        if (QString::fromNSString(item.title) == text)
            return item;
        if (auto *found = findItem(item.submenu, text))
            return found;
    }
    return nil;
}
} // namespace

quintptr macApplicationDelegate()
{
    return reinterpret_cast<quintptr>(NSApp.delegate);
}

QStringList macDockMenuLabels()
{
    @autoreleasepool {
        NSMenu *menu = [NSApp.delegate applicationDockMenu:NSApp];
        QStringList labels;
        for (NSMenuItem *item in menu.itemArray)
            if (!item.isSeparatorItem)
                labels.append(QString::fromNSString(item.title));
        return labels;
    }
}

bool dispatchMacDockAction(const QString &text)
{
    id target = nil;
    SEL action = nullptr;
    @autoreleasepool {
        NSMenu *menu = [NSApp.delegate applicationDockMenu:NSApp];
        NSMenuItem *item = findItem(menu, text);
        if (!item || !item.isEnabled)
            return false;
        target = item.target;
        action = item.action;
    }
    // Like the Dock process: the serialized menu has gone away, and sender is nil.
    return [NSApp sendAction:action to:target from:nil];
}

bool dispatchMacMenuAction(QMenu *menu, const QString &text)
{
    NSMenu *native = menu->toNSMenu();
    for (NSInteger index = 0; index < native.numberOfItems; ++index) {
        if (QString::fromNSString([native itemAtIndex:index].title) == text) {
            [native performActionForItemAtIndex:index];
            return true;
        }
    }
    return false;
}
