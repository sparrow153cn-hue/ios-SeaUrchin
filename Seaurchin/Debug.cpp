#include "Debug.h"

using namespace std;

#if defined(_WIN32)

static HANDLE   hConsoleOutput = nullptr,
                hConsoleInput = nullptr,
                hConsoleError = nullptr;

static DWORD dwDummy;

void _InitializeDebugFeature()
{
    AllocConsole();
    hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
    hConsoleError = GetStdHandle(STD_ERROR_HANDLE);
}

void _TerminateDebugFeature()
{
    FreeConsole();
}

void _WriteDebugConsoleA(const char *string)
{
    WriteConsoleA(hConsoleOutput, string, strlen(string), &dwDummy, nullptr);
}

void _WriteDebugConsoleW(const wchar_t *string)
{
    WriteConsoleW(hConsoleOutput, string, wcslen(string), &dwDummy, nullptr);
}

//AngelScript用
void _WriteDebugConsoleU(const string& message)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, nullptr, 0);
    wchar_t *buffer = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, buffer, len);
    WriteConsoleW(hConsoleOutput, buffer, wcslen(buffer), &dwDummy, nullptr);
    delete[] buffer;
}

#else // POSIX: the debug "console" is stderr (shown in Xcode's console)

void _InitializeDebugFeature() {}
void _TerminateDebugFeature() {}

void _WriteDebugConsoleA(const char *string)
{
    fputs(string, stderr);
}

void _WriteDebugConsoleW(const wchar_t *string)
{
    fputs(sea::WideToUtf8(string).c_str(), stderr);
}

void _WriteDebugConsoleU(const string& message)
{
    fputs(message.c_str(), stderr);
}

#endif

void Debug_ReleaseFunction(...)
{
    
}

// Release-build stand-in for _WriteDebugConsoleU (see Debug.h): same signature, does nothing.
void Debug_ReleaseFunctionU(const string&)
{

}
