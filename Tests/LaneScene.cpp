// Replays the draw-call sequence of ScenePlayer::Draw() (copied from
// Seaurchin/ScenePlayer.Draw.cpp) through the DxLib replacement and the CPU
// rasterizer, using the real default-skin images, and writes lane.ppm.
// This verifies camera, UV mapping, render-target compositing and blend modes
// end to end without a GPU.
#include "../Platform/DxCompat/DxLib.h"
#include "DxSoftRenderer.h"
#include "TestUtil.h"
#include "../Platform/SeaPng.h"

#include <fstream>
#include <iterator>

using namespace sea;

#define SU_LANE_X_MIN -400.0
#define SU_LANE_X_MAX 400.0
#define SU_LANE_Z_MIN_EXT -300.0
#define SU_LANE_Z_MIN 0.0
#define SU_LANE_Z_MAX 3000.0
#define SU_LANE_Y_GROUND 0.0
#define SU_LANE_Y_AIRINDICATE 160.0
#define SU_LANE_ASPECT ((SU_LANE_Z_MAX - SU_LANE_Z_MIN) / (SU_LANE_X_MAX - SU_LANE_X_MIN))
#define SU_LANE_ASPECT_EXT ((SU_LANE_Z_MAX - SU_LANE_Z_MIN_EXT) / (SU_LANE_X_MAX - SU_LANE_X_MIN))

static const std::string kSkin = "../Resources/Skins/Default/Images/";
static double lerpd(double x, double a, double b) { return (1.0 - x) * a + x * b; }

static const uint16_t RectVertexIndices[] = { 0, 1, 3, 3, 1, 2 };
static double laneBufferX = 1024, laneBufferY = laneBufferX * SU_LANE_ASPECT, widthPerLane = laneBufferX / 16;
static double cullingLimit = SU_LANE_ASPECT_EXT / SU_LANE_ASPECT, noteImageBlockX = 64, noteImageBlockY = 64;
static double actualNoteScaleX = (widthPerLane / 2) / noteImageBlockX, actualNoteScaleY = actualNoteScaleX * 2.0;
static double cameraZ = -340, cameraY = 620, cameraTargetZ = 580;

static void DrawTap(int lane, int length, double relpos, int handle)
{   // verbatim from ScenePlayer::DrawTap
    for (int i = 0; i < length * 2; i++) {
        int type = i ? (i == length * 2 - 1 ? 2 : 1) : 0;
        DrawRectRotaGraph3F((lane * 2 + i) * widthPerLane / 2, laneBufferY * relpos, noteImageBlockX * type, (0),
            noteImageBlockX, noteImageBlockY, 0, noteImageBlockY / 2, actualNoteScaleX, actualNoteScaleY, 0, handle, TRUE, FALSE);
    }
}

static void Prepare3DDrawCall()
{
    SetUseLighting(FALSE);
    SetCameraPositionAndTarget_UpVecY(VGet(0, cameraY, cameraZ), VGet(0, SU_LANE_Y_GROUND, cameraTargetZ));
}

int main()
{
    DxSoftRenderer r(1280, 720);
    DxInitialize(&r, 1280, 720, DecodePng);
    SetUseZBuffer3D(TRUE); SetWriteZBuffer3D(TRUE);
    SetDrawScreen(DX_SCREEN_BACK);

    auto L = [&](const char *n) { int h = LoadGraph((kSkin + n).c_str()); if (h < 0) { std::printf("cannot load %s\n", n); std::exit(2); } return h; };
    int imgBack = L("Background.png"), imgCover = L("PlayerTopCover.png"), ground = L("Lane-Ground.png"), judge = L("Lane-JudgeLine.png");
    int tap = L("Note-Tap.png"), extap = L("Note-ExTap.png"), flick = L("Note-Flick.png"), hold = L("Note-Hold.png"), strut = L("Note-HoldStrut.png");
    int airUp = L("Air-Up.png"), airDown = L("Air-Down.png");

    // --- ScenePlayer::LoadResources
    int exty = int(laneBufferX * SU_LANE_ASPECT_EXT);
    double bufferY = 2; while (exty > bufferY) bufferY *= 2;
    float bufferV = float(exty / bufferY);
    VERTEX3D GroundVertices[] = {
        { VGet(SU_LANE_X_MIN, SU_LANE_Y_GROUND, SU_LANE_Z_MAX), VGet(0, 1, 0), GetColorU8(255, 255, 255, 255), GetColorU8(0, 0, 0, 0), 0.0f, 0.0f, 0.0f, 0.0f },
        { VGet(SU_LANE_X_MAX, SU_LANE_Y_GROUND, SU_LANE_Z_MAX), VGet(0, 1, 0), GetColorU8(255, 255, 255, 255), GetColorU8(0, 0, 0, 0), 1.0f, 0.0f, 0.0f, 0.0f },
        { VGet(SU_LANE_X_MAX, SU_LANE_Y_GROUND, SU_LANE_Z_MIN_EXT), VGet(0, 1, 0), GetColorU8(255, 255, 255, 255), GetColorU8(0, 0, 0, 0), 1.0f, 1.0f, 0.0f, 0.0f },
        { VGet(SU_LANE_X_MIN, SU_LANE_Y_GROUND, SU_LANE_Z_MIN_EXT), VGet(0, 1, 0), GetColorU8(255, 255, 255, 255), GetColorU8(0, 0, 0, 0), 0.0f, 1.0f, 0.0f, 0.0f } };
    for (int i = 2; i < 4; i++) GroundVertices[i].v = bufferV;
    int hGroundBuffer = MakeScreen(int(laneBufferX), int(bufferY), TRUE);
    std::printf("ground buffer %dx%d, v=%.4f\n", int(laneBufferX), int(bufferY), bufferV);

    for (int frame = 0; frame < 2; frame++) {          // 2nd frame proves state carries over cleanly
        ClearDrawScreen();
        // spBack (z=0)
        SetDrawBright(255, 255, 255); SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawRotaGraph3F(0, 0, 0, 0, 1, 1, 0, imgBack, TRUE, FALSE);

        // --- ScenePlayer::Draw (z=5)
        SetDrawScreen(hGroundBuffer);
        ClearDrawScreen();
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawGraph(0, 0, ground, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
        DrawRectRotaGraph3F(0, float(laneBufferY), 0, 0, 1024, 64, 0, 32, 1, 1, 0, judge, TRUE, FALSE);

        // DrawMeasureLines
        int division = 8;
        for (int i = 1; i < division; i++) DrawLineAA(float(laneBufferX / division * i), 0, float(laneBufferX / division * i), float(laneBufferY * cullingLimit), GetColor(255, 255, 255), 3);
        for (double rp : { 0.85, 0.45 }) DrawLineAA(0, float(rp * laneBufferY), float(laneBufferX), float(rp * laneBufferY), GetColor(255, 255, 255), 6);

        // notes (DrawHoldNotes / DrawShortNotes)
        SetDrawBlendMode(DX_BLENDMODE_ADD, 255);
        DrawModiGraphF(float(4 * widthPerLane), float(laneBufferY * 0.70), float(8 * widthPerLane), float(laneBufferY * 0.70),
                       float(8 * widthPerLane), float(laneBufferY * 0.40), float(4 * widthPerLane), float(laneBufferY * 0.40), strut, TRUE);
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawTap(4, 4, 0.70, hold); DrawTap(4, 4, 0.40, hold);
        DrawTap(1, 3, 0.90, tap);
        DrawTap(6, 2, 0.62, extap);
        DrawTap(11, 3, 0.80, flick);
        DrawTap(9, 4, 0.30, tap);
        SetDrawScreen(DX_SCREEN_BACK);

        Prepare3DDrawCall();
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawPolygonIndexed3D(GroundVertices, 4, RectVertexIndices, 2, hGroundBuffer, TRUE);

        // 3D air notes (DrawAirNotes)
        Prepare3DDrawCall();
        for (double relpos : { 0.92, 0.75, 0.50 }) {
            double z = lerpd(relpos, SU_LANE_Z_MAX, SU_LANE_Z_MIN);
            int slane = relpos > 0.7 ? 7 : 2, length = 3;
            auto left = lerpd(slane / 16.0, SU_LANE_X_MIN, SU_LANE_X_MAX), right = lerpd((slane + length) / 16.0, SU_LANE_X_MIN, SU_LANE_X_MAX);
            double role = 0.25;
            int handle = relpos > 0.6 ? airUp : airDown;
            VERTEX3D vertices[] = {
                { VGet(left, SU_LANE_Y_AIRINDICATE, z), VGet(0, 0, -1), GetColorU8(255, 255, 255, 255), GetColorU8(0, 0, 0, 0), 0.0f, role, 0.0f, 0.0f },
                { VGet(right, SU_LANE_Y_AIRINDICATE, z), VGet(0, 0, -1), GetColorU8(255, 255, 255, 255), GetColorU8(0, 0, 0, 0), 1.0f, role, 0.0f, 0.0f },
                { VGet(right, SU_LANE_Y_GROUND, z), VGet(0, 0, -1), GetColorU8(255, 255, 255, 255), GetColorU8(0, 0, 0, 0), 1.0f, role + 0.5f, 0.0f, 0.0f },
                { VGet(left, SU_LANE_Y_GROUND, z), VGet(0, 0, -1), GetColorU8(255, 255, 255, 255), GetColorU8(0, 0, 0, 0), 0.0f, role + 0.5f, 0.0f, 0.0f } };
            Prepare3DDrawCall();
            SetUseZBuffer3D(FALSE);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
            DrawPolygonIndexed3D(vertices, 4, RectVertexIndices, 2, handle, TRUE);
        }
        SetUseZBuffer3D(TRUE);

        // spTopCover (z=10)
        SetDrawBright(255, 255, 255); SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
        DrawRotaGraph3F(0, 0, 0, 0, 1, 1, 0, imgCover, TRUE, FALSE);
        ScreenFlip();
    }
    r.SaveMainPPM("/tmp/lane.ppm");
    std::printf("draw calls in last run: %d, textures alive: %d\n", r.drawCalls, r.liveTextures());
    DxShutdown();
    return 0;
}
