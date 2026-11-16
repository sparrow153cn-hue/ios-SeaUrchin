#import <Foundation/Foundation.h>

// The engine reads its skins/scripts/images from <root>/Data (the Windows build copies the
// repository's Resources folder there after building).  On iOS the bundle is read-only, so the
// bundled "Resources" folder is copied to <Documents>/Data on first launch and after every app update.
// Files the user added to Documents/Data (extra skins etc.) are kept; bundled files are overwritten.
@interface SeaResourceStaging : NSObject
+ (BOOL)stageBundledDataInto:(NSString *)documentsDirectory error:(NSError **)error;
@end
