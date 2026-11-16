#import "SeaResourceStaging.h"

@implementation SeaResourceStaging

+ (BOOL)stageBundledDataInto:(NSString *)documentsDirectory error:(NSError **)error
{
    NSFileManager *fm = NSFileManager.defaultManager;
    NSString *src = [NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:@"Resources"];
    NSString *dst = [documentsDirectory stringByAppendingPathComponent:@"Data"];
    NSString *marker = [documentsDirectory stringByAppendingPathComponent:@".seaurchin-staged"];

    BOOL srcIsDir = NO;
    if (![fm fileExistsAtPath:src isDirectory:&srcIsDir] || !srcIsDir) {
        if (error) *error = [NSError errorWithDomain:@"Seaurchin" code:1
                                            userInfo:@{ NSLocalizedDescriptionKey: @"Bundled Resources folder is missing (add it to the app target as a folder reference)" }];
        return NO;
    }

    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    NSString *version = [NSString stringWithFormat:@"%@+%@", info[@"CFBundleShortVersionString"], info[@"CFBundleVersion"]];
    NSString *previous = [NSString stringWithContentsOfFile:marker encoding:NSUTF8StringEncoding error:nil];
#if !DEBUG
    if ([fm fileExistsAtPath:dst] && [previous isEqualToString:version]) return YES;
#endif                                                    // debug builds restage every launch so edits to skins are picked up

    if (![fm createDirectoryAtPath:dst withIntermediateDirectories:YES attributes:nil error:error]) return NO;

    NSDirectoryEnumerator *walker = [fm enumeratorAtPath:src];
    for (NSString *relative in walker) {
        NSString *from = [src stringByAppendingPathComponent:relative];
        NSString *to = [dst stringByAppendingPathComponent:relative];
        BOOL isDir = NO;
        [fm fileExistsAtPath:from isDirectory:&isDir];
        if (isDir) {
            if (![fm createDirectoryAtPath:to withIntermediateDirectories:YES attributes:nil error:error]) return NO;
            continue;
        }
        [fm createDirectoryAtPath:[to stringByDeletingLastPathComponent] withIntermediateDirectories:YES attributes:nil error:nil];
        if ([fm fileExistsAtPath:to]) [fm removeItemAtPath:to error:nil];
        if (![fm copyItemAtPath:from toPath:to error:error]) return NO;
    }
    [version writeToFile:marker atomically:YES encoding:NSUTF8StringEncoding error:nil];
    return YES;
}

@end
