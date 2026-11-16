#pragma once
// GameHost - the platform-independent replacement for Main.cpp's
// PreInitialize/Initialize/Run/Terminate, for hosts (iOS) that own the run loop.
// All functions must be called from the main thread unless noted.

#include <string>
#include "../Platform/DxCompat/DxLib.h"     // sea::DxImageDecoder
#include "../Platform/DxCompat/DxRenderer.h"

namespace sea {

struct GameHostConfig {
    std::string rootDirectory;              // UTF-8. Contains Data/, Music/, Cache/, config.json
    DxRenderer *renderer = nullptr;         // not owned
    DxImageDecoder imageDecoder;            // PNG/JPEG/... -> RGBA8
    int width = 1280;                       // virtual resolution (SU_RES_WIDTH/HEIGHT)
    int height = 720;
};

bool GameHostStart(const GameHostConfig &config);
void GameHostFrame();                       // once per display refresh: Tick + Draw (+ Present)
void GameHostStop();
void GameHostSuspend();                     // app left the foreground: pause audio
void GameHostResume();

// Touch input.  x is normalised across the slider area (0 = left edge, 1 = right edge).
// Thread-safe.
void GameHostTouchBegan(int fingerId, double x);
void GameHostTouchMoved(int fingerId, double x);
void GameHostTouchEnded(int fingerId);

bool GameHostExitRequested();

} // namespace sea
