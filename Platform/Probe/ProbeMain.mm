// Probe app: a deliberately tiny app used to bisect "the real app crashes on launch under LiveContainer".
//
//   SeaurchinProbe      - UIKit only (plus the crash reporter).  No BASS, no Metal, no engine.
//   SeaurchinProbeBass  - the same, but also links and embeds the BASS frameworks and calls BASS_GetVersion().
//
// If a probe shows a green screen with text, everything it uses (IPA packaging, signing, the loader) works.
#import <UIKit/UIKit.h>
#import "SeaCrashReporter.h"

#if SEA_PROBE_BASS
#include <bass.h>
#endif

@interface ProbeDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation ProbeDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    SeaCrashReporterBreadcrumb("probe didFinishLaunching");

    NSMutableString *text = [NSMutableString stringWithString:@"Seaurchin probe\n\nThe app launched fine, so the IPA, its signing and the loader all work."];
#if SEA_PROBE_BASS
    [text appendFormat:@"\n\nBASS is loaded: version %08x", (unsigned int)BASS_GetVersion()];
#else
    [text appendString:@"\n\n(This variant does not use BASS.)"];
#endif
    NSString *previous = SeaCrashReporterTakePreviousReport();
    if (previous.length > 0) [text appendFormat:@"\n\nPrevious crash:\n%@", previous];

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController *vc = [[UIViewController alloc] init];
    vc.view.backgroundColor = UIColor.systemGreenColor;
    UILabel *label = [[UILabel alloc] initWithFrame:CGRectInset(vc.view.bounds, 24, 48)];
    label.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    label.numberOfLines = 0;
    label.textAlignment = NSTextAlignmentCenter;
    label.textColor = UIColor.blackColor;
    label.text = text;
    [vc.view addSubview:label];
    self.window.rootViewController = vc;
    [self.window makeKeyAndVisible];
    return YES;
}

@end

int main(int argc, char *argv[])
{
    SeaCrashReporterInstall();
    SeaCrashReporterBreadcrumb("probe main() entered");
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([ProbeDelegate class]));
    }
}
