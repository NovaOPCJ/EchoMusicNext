#import <Cocoa/Cocoa.h>

// Independent AppKit application: an auxiliary window in the tested process is
// not evidence that an overlay can join another application's fullscreen Space.
@interface EchoFullscreenHost : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property(strong) NSWindow *window;
@property(copy) NSString *directory;
@property BOOL stopping;
@end

@implementation EchoFullscreenHost
- (void)publish:(BOOL)fullscreen
{
    NSDictionary *state = @{
        @"fullscreen" : @(fullscreen),
        @"pid" : @(NSProcessInfo.processInfo.processIdentifier),
        @"window" : @(self.window.windowNumber)
    };
    NSData *data = [NSJSONSerialization dataWithJSONObject:state options:0 error:nil];
    [data writeToFile:[self.directory stringByAppendingPathComponent:@"state.json"] atomically:YES];
}
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;
    self.window = [[NSWindow alloc]
        initWithContentRect:NSMakeRect(100, 100, 800, 600)
                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                            NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                    backing:NSBackingStoreBuffered
                      defer:NO];
    self.window.delegate = self;
    self.window.title = @"EchoMusicNext fullscreen verification";
    self.window.backgroundColor = [NSColor colorWithRed:.08 green:.16 blue:.26 alpha:1];
    self.window.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    [self publish:NO];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 4), dispatch_get_main_queue(), ^{
      [self.window toggleFullScreen:nil];
    });
    [NSTimer scheduledTimerWithTimeInterval:.1
                                    repeats:YES
                                      block:^(NSTimer *) {
                                        NSString *raise = [self.directory
                                            stringByAppendingPathComponent:@"raise"];
                                        if ([NSFileManager.defaultManager fileExistsAtPath:raise]) {
                                            [self.window orderFrontRegardless];
                                            [NSFileManager.defaultManager removeItemAtPath:raise
                                                                                     error:nil];
                                        }
                                        if (!self.stopping &&
                                            [NSFileManager.defaultManager
                                                fileExistsAtPath:
                                                    [self.directory
                                                        stringByAppendingPathComponent:@"stop"]]) {
                                            self.stopping = YES;
                                            if (self.window.styleMask & NSWindowStyleMaskFullScreen)
                                                [self.window toggleFullScreen:nil];
                                            else
                                                [NSApp terminate:nil];
                                        }
                                      }];
    // A failed parent test must never leave a fullscreen host behind.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 60 * NSEC_PER_SEC), dispatch_get_main_queue(),
                   ^{
                     [NSApp terminate:nil];
                   });
}
- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    (void)notification;
    [self publish:YES];
}
- (void)windowDidExitFullScreen:(NSNotification *)notification
{
    (void)notification;
    [self publish:NO];
    if (self.stopping)
        [NSApp terminate:nil];
}
@end

int main(int argc, char **argv)
{
    @autoreleasepool {
        if (argc != 2)
            return 2;
        NSApplication *app = NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        EchoFullscreenHost *delegate __attribute__((objc_precise_lifetime)) =
            [EchoFullscreenHost new];
        delegate.directory = [NSString stringWithUTF8String:argv[1]];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
