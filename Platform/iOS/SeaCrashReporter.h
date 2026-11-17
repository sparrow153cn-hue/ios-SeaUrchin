// SeaCrashReporter.h - records why the app died, and shows it on the next launch.
//
// Needed because the app is run unsigned through LiveContainer / sideloading, where there is no Xcode console:
// a crash leaves no trace.  This writes Documents/seaurchin-crash.txt (signal + backtrace, uncaught Objective-C
// exception, or the message of an uncaught C++ exception) and Documents/seaurchin-startup.log (breadcrumbs).
// The next launch reads both, deletes them and hands the text to SeaAppDelegate to show in an alert.
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C" {
#endif

// Installs the handlers.  Safe to call more than once; also runs automatically as early as possible at load time.
void SeaCrashReporterInstall(void);

// Appends one line to the start-up log (kept only until the next launch).
void SeaCrashReporterBreadcrumb(const char *text);

// The report of the previous run if it crashed, else nil.  Returns it only once.
NSString *SeaCrashReporterTakePreviousReport(void);

#ifdef __cplusplus
}
#endif
