#include "SceneDebug.h"
#include "Debug.h"

SceneDebug::~SceneDebug()
{
    WriteDebugConsole("SceneDebug Destructed.\n");
}

void SceneDebug::Tick(double delta)
{
    calc += delta;
    call++;
    if (calc >= 1.00)
    {
        fps = call / calc;
        calc = call = 0;
    }
}

void SceneDebug::Draw()
{
#if defined(_WIN32)
    clsDx();
    printfDx(reinterpret_cast<const char*>(L"%2.1f fps\n"), fps);
#endif
    // Other platforms: DxLib's on-screen debug text does not exist; the host shows its own FPS counter.
}

bool SceneDebug::IsDead()
{
    return false;
}

