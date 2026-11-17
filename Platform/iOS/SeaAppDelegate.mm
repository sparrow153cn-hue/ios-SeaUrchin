#import "SeaAppDelegate.h"
#import "SeaGameViewController.h"
#import "SeaCrashReporter.h"

@implementation SeaAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    SeaCrashReporterBreadcrumb("didFinishLaunching");
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.backgroundColor = UIColor.blackColor;
    application.idleTimerDisabled = YES;      // never dim the screen mid-song

    NSString *previous = SeaCrashReporterTakePreviousReport();
    if (previous.length > 0) {
        // The last run crashed.  Show why before starting the game (which may well crash again), so the text can be
        // read and copied.
        UIViewController *placeholder = [[UIViewController alloc] init];
        placeholder.view.backgroundColor = UIColor.blackColor;
        self.window.rootViewController = placeholder;
        [self.window makeKeyAndVisible];

        NSString *shown = previous.length > 2500
            ? [[previous substringToIndex:2500] stringByAppendingString:@"\n...(use Copy report for the rest)"]
            : previous;
        UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"The last run crashed"
                                                                      message:shown
                                                               preferredStyle:UIAlertControllerStyleAlert];
        __weak SeaAppDelegate *weakSelf = self;
        [alert addAction:[UIAlertAction actionWithTitle:@"Copy report" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action) {
            UIPasteboard.generalPasteboard.string = previous;
            [weakSelf startGame];
        }]];
        [alert addAction:[UIAlertAction actionWithTitle:@"Continue" style:UIAlertActionStyleCancel handler:^(UIAlertAction *action) {
            [weakSelf startGame];
        }]];
        dispatch_async(dispatch_get_main_queue(), ^{
            [placeholder presentViewController:alert animated:YES completion:nil];
        });
        return YES;
    }

    [self startGame];
    return YES;
}

- (void)startGame
{
    SeaCrashReporterBreadcrumb("creating SeaGameViewController");
    self.window.rootViewController = [[SeaGameViewController alloc] init];
    [self.window makeKeyAndVisible];
    SeaCrashReporterBreadcrumb("game view controller is on screen");
}

@end
