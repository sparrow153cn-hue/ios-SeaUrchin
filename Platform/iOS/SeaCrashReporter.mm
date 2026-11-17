// SeaCrashReporter.mm - see SeaCrashReporter.h.
//
// NOTE: written without access to Xcode.  The signal handler only uses open/write/dprintf/backtrace, which is not
// strictly async-signal-safe but works in practice and is what most crash loggers do.
#import "SeaCrashReporter.h"

#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <exception>
#include <string>

static char gCrashPath[1024];
static char gLogPath[1024];
static NSString *gPreviousReport = nil;
static bool gInstalled = false;

static void AppendToFile(const char *path, const char *text)
{
    if (!path[0]) return;
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    (void)write(fd, text, strlen(text));
    close(fd);
}

static void SignalHandler(int sig, siginfo_t *info, void *context)
{
    int fd = gCrashPath[0] ? open(gCrashPath, O_WRONLY | O_CREAT | O_APPEND, 0644) : -1;
    if (fd >= 0) {
        dprintf(fd, "Fatal signal %d (%s), fault address %p\n", sig, strsignal(sig), info ? info->si_addr : NULL);
        void *frames[64];
        int n = backtrace(frames, 64);
        backtrace_symbols_fd(frames, n, fd);
        close(fd);
    }
    signal(sig, SIG_DFL);      // let the system finish the crash as usual
    raise(sig);
}

static void TerminateHandler()
{
    std::string msg = "std::terminate called";
    std::exception_ptr ep = std::current_exception();
    if (ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception &e) {
            msg += " with C++ exception: ";
            msg += e.what();
        } catch (...) {
            msg += " with a non-std C++ exception";
        }
    } else {
        msg += " (no active exception)";
    }
    msg += "\n";
    AppendToFile(gCrashPath, msg.c_str());
    abort();                   // -> SIGABRT -> SignalHandler appends the backtrace
}

static void UncaughtExceptionHandler(NSException *e)
{
    NSString *text = [NSString stringWithFormat:@"Uncaught Objective-C exception: %@\nReason: %@\n%@\n",
                      e.name, e.reason, [e.callStackSymbols componentsJoinedByString:@"\n"]];
    AppendToFile(gCrashPath, text.UTF8String);
}

void SeaCrashReporterBreadcrumb(const char *text)
{
    AppendToFile(gLogPath, text);
    AppendToFile(gLogPath, "\n");
}

void SeaCrashReporterInstall(void)
{
    if (gInstalled) return;
    gInstalled = true;

    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    if (!docs) return;
    NSString *crashFile = [docs stringByAppendingPathComponent:@"seaurchin-crash.txt"];
    NSString *logFile = [docs stringByAppendingPathComponent:@"seaurchin-startup.log"];
    strlcpy(gCrashPath, crashFile.fileSystemRepresentation, sizeof(gCrashPath));
    strlcpy(gLogPath, logFile.fileSystemRepresentation, sizeof(gLogPath));

    // A crash file left over from the previous run: keep its text for the alert, then start clean.
    NSString *previousCrash = [NSString stringWithContentsOfFile:crashFile encoding:NSUTF8StringEncoding error:nil];
    if (previousCrash.length > 0) {
        NSString *previousLog = [NSString stringWithContentsOfFile:logFile encoding:NSUTF8StringEncoding error:nil];
        gPreviousReport = [NSString stringWithFormat:@"%@\n--- last start-up steps ---\n%@",
                           previousCrash, previousLog.length > 0 ? previousLog : @"(none)"];
    }
    [NSFileManager.defaultManager removeItemAtPath:crashFile error:nil];
    [NSFileManager.defaultManager removeItemAtPath:logFile error:nil];

    NSSetUncaughtExceptionHandler(&UncaughtExceptionHandler);
    std::set_terminate(&TerminateHandler);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = SignalHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    const int signals[] = { SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGTRAP };
    for (int s : signals) sigaction(s, &sa, NULL);

    SeaCrashReporterBreadcrumb("crash reporter installed");
}

NSString *SeaCrashReporterTakePreviousReport(void)
{
    NSString *report = gPreviousReport;
    gPreviousReport = nil;
    return report;
}

// Runs when the executable is loaded, i.e. before main() and before most static initialisers, so a crash during
// start-up is caught too.
__attribute__((constructor)) static void SeaCrashReporterEarly(void)
{
    SeaCrashReporterInstall();
}
