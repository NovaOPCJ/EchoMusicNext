#include "BrowserWindowLauncher.h"
#import <AppKit/AppKit.h>

namespace EchoMusic
{
QString defaultBrowserExecutable()
{
    @autoreleasepool {
        // Resolve only the handler, never send the one-use verification URL to
        // another app until the caller has selected its launch mode.
        NSURL *app = [NSWorkspace.sharedWorkspace
            URLForApplicationToOpenURL:[NSURL URLWithString:@"http://127.0.0.1/"]];
        return app ? QString::fromNSString([NSBundle bundleWithURL:app].executablePath) : QString();
    }
}
} // namespace EchoMusic
