// GameHost.cpp - drives ExecutionManager without a Win32 message loop.
#include "GameHost.h"

#include "Config.h"
#include "Debug.h"
#include "Setting.h"
#include "ExecutionManager.h"

#include <atomic>
#include <codecvt>
#include <locale>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"   // std::codecvt_utf8 is deprecated in C++17 but is what Boost.Filesystem wants
#endif

using namespace std;
using namespace std::chrono;

namespace {
    shared_ptr<Setting> g_setting;
    unique_ptr<ExecutionManager> g_manager;
    steady_clock::time_point g_last;
    atomic<bool> g_exitRequested(false);
    bool g_started = false;
}

namespace sea {

void RequestApplicationExit()
{
    // iOS apps must not terminate themselves; the request is recorded for the host to act on (or ignore).
    g_exitRequested = true;
    WriteDebugConsole("ExitApplication() requested\n");
}

bool GameHostExitRequested() { return g_exitRequested; }

bool GameHostStart(const GameHostConfig &config)
{
    if (g_started || !config.renderer) return false;

    // The engine passes std::wstring paths to Boost.Filesystem; make sure the narrow side is UTF-8.
    boost::filesystem::path::imbue(locale(locale(), new codecvt_utf8<wchar_t>()));

    const wstring root = Utf8ToWide(config.rootDirectory);
    {
        // The music scan and settings assume these exist; a fresh install has none of them.
        boost::system::error_code ec;
        boost::filesystem::path p(root);
        boost::filesystem::create_directories(p / SU_DATA_DIR, ec);
        boost::filesystem::create_directories(p / SU_MUSIC_DIR, ec);
        boost::filesystem::create_directories(p / SU_CACHE_DIR, ec);
    }

    g_setting = make_shared<Setting>(root);
    g_setting->Load(SU_SETTING_FILE);

    InitializeDebugFeature();
    DxInitialize(config.renderer, config.width, config.height, config.imageDecoder);
    SetUseZBuffer3D(TRUE);
    SetWriteZBuffer3D(TRUE);
    SetDrawScreen(DX_SCREEN_BACK);

    g_manager = make_unique<ExecutionManager>(g_setting);
    g_manager->Initialize();
    g_manager->EnumerateSkins();
    g_manager->ExecuteSkin();

    g_last = steady_clock::now();
    g_started = true;
    return true;
}

void GameHostFrame()
{
    if (!g_started) return;
    const auto now = steady_clock::now();
    double delta = duration<double>(now - g_last).count();
    g_last = now;
    if (delta > 0.25) delta = 0.25;         // resume-from-background / debugger pause: don't fast-forward animations

    g_manager->Tick(delta);
    g_manager->Draw();                      // ClearDrawScreen ... ScreenFlip
}

void GameHostStop()
{
    if (!g_started) return;
    g_manager->Shutdown();
    g_manager.reset();
    TerminateDebugFeature();
    g_setting->Save();
    g_setting.reset();
    DxShutdown();
    g_started = false;
}

void GameHostSuspend()
{
    if (g_started) BASS_Pause();
}

void GameHostResume()
{
    if (g_started) {
        BASS_Start();
        g_last = steady_clock::now();
    }
}

void GameHostTouchBegan(int id, double x)
{
    if (g_started) g_manager->GetControlStateSafe()->TouchBegan(id, x);
}

void GameHostTouchMoved(int id, double x)
{
    if (g_started) g_manager->GetControlStateSafe()->TouchMoved(id, x);
}

void GameHostTouchEnded(int id)
{
    if (g_started) g_manager->GetControlStateSafe()->TouchEnded(id);
}

} // namespace sea
