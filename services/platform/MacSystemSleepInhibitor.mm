#include "SystemSleepInhibitor.h"
#include <QPointer>
#import <Cocoa/Cocoa.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
namespace EchoMusic
{
struct SystemSleepInhibitor::Native
{
    IOPMAssertionID assertion = kIOPMNullAssertionID;
    id sleepObserver = nil, wakeObserver = nil;
    ~Native()
    {
        auto *center = NSWorkspace.sharedWorkspace.notificationCenter;
        if (sleepObserver) {
            [center removeObserver:sleepObserver];
            [sleepObserver release];
        }
        if (wakeObserver) {
            [center removeObserver:wakeObserver];
            [wakeObserver release];
        }
        if (assertion != kIOPMNullAssertionID)
            IOPMAssertionRelease(assertion);
    }
};
SystemSleepInhibitor::SystemSleepInhibitor(QObject *parent)
    : QObject(parent), m_native(std::make_unique<Native>())
{
    const QPointer<SystemSleepInhibitor> guard(this);
    auto *center = NSWorkspace.sharedWorkspace.notificationCenter;
    m_native->sleepObserver = [[center addObserverForName:NSWorkspaceWillSleepNotification
                                                   object:nil
                                                    queue:NSOperationQueue.mainQueue
                                               usingBlock:^(NSNotification *) {
                                                 if (guard)
                                                     guard->setSystemSuspended(true);
                                               }] retain];
    m_native->wakeObserver = [[center addObserverForName:NSWorkspaceDidWakeNotification
                                                  object:nil
                                                   queue:NSOperationQueue.mainQueue
                                              usingBlock:^(NSNotification *) {
                                                if (guard)
                                                    guard->setSystemSuspended(false);
                                              }] retain];
}
SystemSleepInhibitor::~SystemSleepInhibitor() = default;
void SystemSleepInhibitor::reconcile()
{
    const bool required = m_required && !m_suspended;
    auto &assertion = m_native->assertion;
    if (!required) {
        if (assertion != kIOPMNullAssertionID)
            IOPMAssertionRelease(assertion);
        assertion = kIOPMNullAssertionID;
        publish(false);
    } else if (assertion == kIOPMNullAssertionID) {
        const auto result = IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleSystemSleep, kIOPMAssertionLevelOn,
            CFSTR("EchoMusicNext music playback"), &assertion);
        if (result != kIOReturnSuccess) {
            assertion = kIOPMNullAssertionID;
            publish(false, tr("未能阻止系统休眠（%1）").arg(result));
        } else
            publish(true);
    }
}
} // namespace EchoMusic
