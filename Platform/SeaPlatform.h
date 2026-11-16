// SeaPlatform.h
// Single place where the Windows-specific parts of the original code base are
// either pulled in (on Windows) or replaced (everywhere else, e.g. iOS).
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#if defined(_WIN32)
#   define SEA_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#   include <TargetConditionals.h>
#   if TARGET_OS_IPHONE
#       define SEA_PLATFORM_IOS 1
#   else
#       define SEA_PLATFORM_MACOS 1
#   endif
#endif

#if !defined(SEA_PLATFORM_WINDOWS)
#   define SEA_PLATFORM_POSIX 1
#endif

// ---------------------------------------------------------------------------
// Types / macros that the original sources took from <Windows.h>.
// bass.h defines DWORD/BOOL/TRUE/FALSE itself on non-Windows targets with the
// same definitions, and C++ permits identical typedef/macro redefinition, so
// it is safe for both headers to be included in either order.
// ---------------------------------------------------------------------------
#if defined(SEA_PLATFORM_POSIX)
#   ifndef TRUE
#       define TRUE 1
#   endif
#   ifndef FALSE
#       define FALSE 0
#   endif
    typedef uint32_t DWORD;
#   ifndef __OBJC__
    typedef int BOOL;
#   endif
    // TEXT("...") was used for debug strings only.
#   ifndef TEXT
#       define TEXT(x) x
#   endif
#endif

// Path used to hand a UTF-8 std::string file name to DxLib.
//   Windows : the original code switched DxLib to UTF-16 and passed a wide
//             string cast to const char*.
//   Others  : the DxLib replacement takes plain UTF-8.
#if defined(SEA_PLATFORM_WINDOWS)
#   define SU_DX_PATH(utf8) reinterpret_cast<const char*>(ConvertUTF8ToUnicode(utf8).c_str())
#else
#   define SU_DX_PATH(utf8) ((utf8).c_str())
#endif

// Argument for std::ifstream/ofstream::open(): MSVC accepts std::wstring, every
// other standard library needs a narrow (UTF-8) path.
#if defined(SEA_PLATFORM_WINDOWS)
#   define SU_FS_PATH(wide) (wide)
#else
#   define SU_FS_PATH(wide) (sea::WideToUtf8(wide))
#endif

namespace sea {
    // UTF-8 <-> std::wstring, correct for both 16-bit (Windows, UTF-16) and
    // 32-bit (macOS/iOS/Linux, UTF-32) wchar_t.  Invalid input yields U+FFFD.
    std::wstring Utf8ToWide(const std::string &utf8);
    std::string  WideToUtf8(const std::wstring &wide);
    // Explicit UTF-16 variants (the on-disk / Windows representation); same rules.
    std::u16string Utf8ToUtf16(const std::string &utf8);
    std::string    Utf16ToUtf8(const std::u16string &utf16);

    // Called by the script function ExitApplication().  Implemented by the host (no-op on iOS).
    void RequestApplicationExit();
}
