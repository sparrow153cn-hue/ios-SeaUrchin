#import <UIKit/UIKit.h>
#import "SeaAppDelegate.h"
#import "SeaCrashReporter.h"

int main(int argc, char *argv[])
{
    SeaCrashReporterInstall();
    SeaCrashReporterBreadcrumb("main() entered");
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([SeaAppDelegate class]));
    }
}
