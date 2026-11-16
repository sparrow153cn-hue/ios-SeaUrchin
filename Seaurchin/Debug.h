#pragma once

#include "Config.h"

#if defined(_DEBUG) || defined(DEBUG) || defined(SU_FORCE_LOG)
#if defined(_WIN32) && defined(UNICODE)

#define WriteDebugConsole _WriteDebugConsoleW

#else // multibyte build / non-Windows

#define WriteDebugConsole _WriteDebugConsoleA

#endif

#define InitializeDebugFeature _InitializeDebugFeature
#define TerminateDebugFeature _TerminateDebugFeature
#define WriteDebugConsoleU _WriteDebugConsoleU

#define ASSERT_CALL WriteDebugConsole((string(__func__) + " Called!\n").c_str())

#else // Release

#define WriteDebugConsole Debug_ReleaseFunction
#define InitializeDebugFeature Debug_ReleaseFunction
#define TerminateDebugFeature Debug_ReleaseFunction
#define WriteDebugConsoleU Debug_ReleaseFunction
#define ASSERT_CALL

#endif

void _InitializeDebugFeature();
void _TerminateDebugFeature();

void _WriteDebugConsoleA(const char *string);
void _WriteDebugConsoleW(const wchar_t *string);
void _WriteDebugConsoleU(const std::string& message);
void Debug_ReleaseFunction(...);
