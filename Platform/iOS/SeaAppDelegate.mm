#import "SeaAppDelegate.h"
#import "SeaGameViewController.h"

@implementation SeaAppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[SeaGameViewController alloc] init];
    self.window.backgroundColor = UIColor.blackColor;
    [self.window makeKeyAndVisible];
    application.idleTimerDisabled = YES;      // never dim the screen mid-song
    return YES;
}

@end
