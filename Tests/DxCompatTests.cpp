// Unit tests for DxCore (the DxLib replacement) using the CPU reference renderer.
#include "../Platform/DxCompat/DxLib.h"
#include "DxSoftRenderer.h"
#include "TestUtil.h"

#include <memory>

using namespace sea;

struct Env {
    DxSoftRenderer r{ 1280, 720 };
    explicit Env(DxImageDecoder dec = DecodeRaw) { DxInitialize(&r, 1280, 720, dec); SetDrawScreen(DX_SCREEN_BACK); ClearDrawScreen(); }
    ~Env() { DxShutdown(); }
    bool Is(int x, int y, float R, float G, float B, float eps = 0.01f) const
    {
        auto p = r.MainPixel(x, y);
        return std::fabs(p.r - R) < eps && std::fabs(p.g - G) < eps && std::fabs(p.b - B) < eps;
    }
};

static int RawGraph(const std::vector<uint8_t> &d) { return CreateGraphFromMem(d.data(), (int)d.size()); }

// 2x2: TL red, TR green, BL blue, BR white (opaque)
static std::vector<uint8_t> Rgbw()
{
    return MakeRaw(2, 2, [](int x, int y, uint8_t *p) {
        const uint8_t c[4][3] = { { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }, { 255, 255, 255 } };
        const uint8_t *s = c[y * 2 + x]; p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = 255;
    });
}

static VERTEX3D Vx(float x, float y, float z, float u, float v, int r, int g, int b, int a = 255)
{
    VERTEX3D t; t.pos = VGet(x, y, z); t.norm = VGet(0, 0, -1); t.dif = GetColorU8(r, g, b, a); t.spc = GetColorU8(0, 0, 0, 0);
    t.u = u; t.v = v; t.su = t.sv = 0; return t;
}
static const unsigned short kRectIdx[] = { 0, 1, 3, 3, 1, 2 };     // same as ScenePlayer.Draw.cpp
// vertical quad facing the camera
static void WallQuad(VERTEX3D *q, float x0, float x1, float y0, float y1, float z, int r, int g, int b)
{
    q[0] = Vx(x0, y1, z, 0, 0, r, g, b); q[1] = Vx(x1, y1, z, 1, 0, r, g, b);
    q[2] = Vx(x1, y0, z, 1, 1, r, g, b); q[3] = Vx(x0, y0, z, 0, 1, r, g, b);
}

static void test_flat_box()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    DrawBox(10, 10, 20, 20, GetColor(255, 0, 0), TRUE);
    ScreenFlip();
    CHECK(e.Is(15, 15, 1, 0, 0));
    CHECK(e.Is(10, 10, 1, 0, 0));
    CHECK(e.Is(19, 19, 1, 0, 0));
    CHECK(e.Is(20, 20, 0, 0, 0));                 // right/bottom edge is exclusive, like DxLib
    CHECK(e.Is(9, 15, 0, 0, 0));
    CHECK(e.r.presentCount == 1);
}

static void test_no_double_blend_on_quad_diagonal()
{
    // Two triangles share a diagonal that passes exactly through pixel centres; the top-left
    // rule must make every pixel get covered exactly once or alpha would stack on the diagonal.
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 128);
    DrawBox(100, 100, 164, 164, GetColor(255, 255, 255), TRUE);
    ScreenFlip();
    float lo = 9, hi = -9;
    for (int y = 100; y < 164; y++) for (int x = 100; x < 164; x++) { float v = e.r.MainPixel(x, y).r; lo = std::min(lo, v); hi = std::max(hi, v); }
    CHECK_NEAR(lo, 128 / 255.0, 0.002);
    CHECK_NEAR(hi, 128 / 255.0, 0.002);
}

static void test_texture_orientation_and_turn()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    SetDrawMode(DX_DRAWMODE_NEAREST);
    int g = RawGraph(Rgbw());
    CHECK(g > 0);
    int w = 0, h = 0; GetGraphSize(g, &w, &h); CHECK(w == 2 && h == 2);
    DrawRotaGraph3F(0, 0, 0, 0, 50, 50, 0, g, TRUE, FALSE);          // 100x100 at the origin
    DrawRotaGraph3F(200, 0, 0, 0, 50, 50, 0, g, TRUE, TRUE);         // mirrored
    ScreenFlip();
    CHECK(e.Is(25, 25, 1, 0, 0)); CHECK(e.Is(75, 25, 0, 1, 0));
    CHECK(e.Is(25, 75, 0, 0, 1)); CHECK(e.Is(75, 75, 1, 1, 1));
    CHECK(e.Is(225, 25, 0, 1, 0)); CHECK(e.Is(275, 25, 1, 0, 0));   // turn flag swaps left/right
}

static void test_rotation_is_clockwise_on_screen()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    SetDrawMode(DX_DRAWMODE_NEAREST);
    int g = RawGraph(Rgbw());
    DrawRotaGraph3F(400, 400, 1, 1, 50, 50, 3.14159265358979 / 2, g, TRUE, FALSE);
    ScreenFlip();
    // red started top-left, 90deg clockwise puts it top-right
    CHECK(e.Is(425, 375, 1, 0, 0));
    CHECK(e.Is(425, 425, 0, 1, 0));      // green: top-right -> bottom-right
    CHECK(e.Is(375, 425, 1, 1, 1));      // white: bottom-right -> bottom-left
    CHECK(e.Is(375, 375, 0, 0, 1));      // blue:  bottom-left -> top-left
}

static void test_rect_rota_pivot_is_rect_relative()
{
    // DrawTap() in the game tiles 64px cells taken from a sprite sheet with cx=0, so the pivot
    // must be relative to the SOURCE RECTANGLE, not to the whole image.
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    SetDrawMode(DX_DRAWMODE_NEAREST);
    auto sheet = MakeRaw(192, 64, [](int x, int, uint8_t *p) {
        p[0] = x < 64 ? 255 : 0; p[1] = (x >= 64 && x < 128) ? 255 : 0; p[2] = x >= 128 ? 255 : 0; p[3] = 255; });
    int g = RawGraph(sheet);
    for (int i = 0; i < 3; i++)
        DrawRectRotaGraph3F(100.0f + i * 32, 300, 64 * i, 0, 64, 64, 0, 32, 0.5, 0.5, 0, g, TRUE, FALSE);
    ScreenFlip();
    CHECK(e.Is(116, 300, 1, 0, 0)); CHECK(e.Is(148, 300, 0, 1, 0)); CHECK(e.Is(180, 300, 0, 0, 1));
    CHECK(e.Is(116, 330, 0, 0, 0));      // pivot y=32*0.5 => sprite spans y 284..316
}

static void test_div_graph_and_refcount()
{
    Env e([](const uint8_t *d, size_t s, int *w, int *h, std::vector<uint8_t> *o) { return DecodeRaw(d, s, w, h, o); });
    auto sheet = MakeRaw(4, 2, [](int x, int, uint8_t *p) { p[0] = x < 2 ? 255 : 0; p[1] = x < 2 ? 0 : 255; p[2] = 0; p[3] = 255; });
    std::string path = WriteTemp("sea_div.raw", sheet);
    int hs[2] = { 0, 0 };
    CHECK(LoadDivGraph(path.c_str(), 2, 2, 1, 2, 2, hs) == 0);
    int w, h; GetGraphSize(hs[1], &w, &h); CHECK(w == 2 && h == 2);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255); SetDrawMode(DX_DRAWMODE_NEAREST);
    DrawRotaGraph3F(10, 10, 0, 0, 10, 10, 0, hs[0], TRUE);
    DrawRotaGraph3F(100, 10, 0, 0, 10, 10, 0, hs[1], TRUE);
    ScreenFlip();
    CHECK(e.Is(20, 20, 1, 0, 0)); CHECK(e.Is(110, 20, 0, 1, 0));    // cell 0 red, cell 1 green, no bleeding
    CHECK(e.r.liveTextures() == 1);                                  // one shared texture
    DeleteGraph(hs[0]); CHECK(e.r.liveTextures() == 1);
    DeleteGraph(hs[1]); CHECK(e.r.liveTextures() == 0);
}

static void test_render_target_matches_direct_draw()
{
    // The whole reason for the premultiplied pipeline: compositing through an alpha render
    // target must equal drawing directly.
    auto composite = [](bool viaTarget) {
        Env e;
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);
        DrawBox(0, 0, 400, 400, GetColor(102, 102, 102), TRUE);          // grey background
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 128);
        if (viaTarget) {
            int rt = MakeScreen(64, 64, TRUE);
            SetDrawScreen(rt); ClearDrawScreen();
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 128);
            DrawBox(0, 0, 64, 64, GetColor(255, 0, 0), TRUE);
            SetDrawScreen(DX_SCREEN_BACK);
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
            DrawGraph(100, 100, rt, TRUE);
        } else {
            DrawBox(100, 100, 164, 164, GetColor(255, 0, 0), TRUE);
        }
        ScreenFlip();
        return e.r.MainPixel(120, 120);
    };
    auto a = composite(false), b = composite(true);
    CHECK_NEAR(a.r, 0.5 + 0.5 * 0.4, 0.01); CHECK_NEAR(a.g, 0.5 * 0.4, 0.01);
    CHECK_NEAR(b.r, a.r, 0.005); CHECK_NEAR(b.g, a.g, 0.005); CHECK_NEAR(b.b, a.b, 0.005);
}

static void test_render_target_without_alpha_is_opaque()
{
    Env e;
    int rt = MakeScreen(32, 32, FALSE);
    SetDrawScreen(rt); ClearDrawScreen();
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);
    DrawBox(0, 0, 32, 32, GetColor(255, 255, 255), TRUE);
    SetDrawScreen(DX_SCREEN_BACK);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    DrawGraph(50, 50, rt, TRUE);
    ScreenFlip();
    CHECK(e.Is(60, 60, 1, 1, 1));
}

static void test_blend_add_and_bright()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);
    DrawBox(0, 0, 100, 100, GetColor(51, 51, 51), TRUE);              // 0.2
    SetDrawBlendMode(DX_BLENDMODE_ADD, 128);
    DrawBox(10, 10, 50, 50, GetColor(255, 255, 255), TRUE);
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    SetDrawBright(128, 255, 0);
    DrawBox(200, 200, 220, 220, GetColor(255, 255, 255), TRUE);
    SetDrawBright(255, 255, 255);
    ScreenFlip();
    CHECK(e.Is(30, 30, 0.2f + 0.502f, 0.2f + 0.502f, 0.2f + 0.502f, 0.01f));
    CHECK(e.Is(70, 70, 0.2f, 0.2f, 0.2f));
    CHECK(e.Is(210, 210, 0.502f, 1, 0));
}

static void test_shapes()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    DrawLineAA(10, 10, 110, 10, GetColor(255, 255, 255), 4);
    DrawOvalAA(500, 500, 50, 50, 256, GetColor(255, 255, 255), TRUE);
    DrawOvalAA(800, 500, 50, 50, 256, GetColor(255, 255, 255), FALSE, 4);
    DrawBoxAA(300, 300, 400, 400, GetColor(0, 255, 0), FALSE, 4);
    DrawPixel(5, 600, GetColor(255, 0, 255));
    ScreenFlip();
    CHECK(e.Is(60, 10, 1, 1, 1)); CHECK(e.Is(60, 20, 0, 0, 0));
    CHECK(e.Is(500, 500, 1, 1, 1)); CHECK(e.Is(560, 500, 0, 0, 0));
    CHECK(e.Is(800, 500, 0, 0, 0)); CHECK(e.Is(849, 500, 1, 1, 1)); CHECK(e.Is(800, 450, 1, 1, 1));
    CHECK(e.Is(350, 300, 0, 1, 0)); CHECK(e.Is(350, 350, 0, 0, 0));
    CHECK(e.Is(5, 600, 1, 0, 1));
}

static void test_camera_projection_matches_reference()
{
    Env e;
    SetCameraPositionAndTarget_UpVecY(VGet(0, 620, -340), VGet(0, 0, 580));
    VECTOR judge = ConvWorldPosToScreenPos(VGet(0, 0, 0));
    VECTOR farL = ConvWorldPosToScreenPos(VGet(-400, 0, 3000));
    VECTOR farR = ConvWorldPosToScreenPos(VGet(400, 0, 3000));
    // Reference numbers come from an independent numpy implementation (60deg vertical FOV).
    CHECK_NEAR(judge.x, 640.0, 0.2); CHECK_NEAR(judge.y, 681.6, 0.3);
    CHECK_NEAR(farL.x, 560.0, 0.3);  CHECK_NEAR(farL.y, 89.4, 0.3);
    CHECK_NEAR(farR.x, 720.0, 0.3);
    CHECK(judge.z > 0 && judge.z < 1);
    VECTOR behind = ConvWorldPosToScreenPos(VGet(0, 620, -1000));
    CHECK(behind.z < 0);
}

static void test_3d_ground_quad_and_depth()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    SetCameraPositionAndTarget_UpVecY(VGet(0, 620, -340), VGet(0, 0, 580));
    VERTEX3D ground[4] = {
        Vx(-400, 0, 3000, 0, 0, 255, 255, 255), Vx(400, 0, 3000, 1, 0, 255, 255, 255),
        Vx(400, 0, -300, 1, 1, 255, 255, 255),  Vx(-400, 0, -300, 0, 1, 255, 255, 255) };
    DrawPolygonIndexed3D(ground, 4, kRectIdx, 2, 0, TRUE);
    ScreenFlip();
    CHECK(e.Is(640, 400, 1, 1, 1));       // inside the lane
    CHECK(e.Is(50, 400, 0, 0, 0));        // outside it
    CHECK(e.Is(640, 60, 0, 0, 0));        // beyond the far end (y ~ 89)
    VECTOR p = ConvWorldPosToScreenPos(VGet(0, 0, 1500));
    CHECK(e.Is((int)p.x, (int)p.y, 1, 1, 1));

    // depth test: nearer red wall must win regardless of draw order; without z-buffer, order wins.
    VERTEX3D nearRed[4], farGreen[4];
    WallQuad(nearRed, -100, 100, 0, 200, 500, 255, 0, 0);
    WallQuad(farGreen, -1500, 1500, -3000, 3000, 1500, 0, 255, 0);   // must cover the probe pixel (ray hits z=1500 at y~-519)
    VECTOR at = ConvWorldPosToScreenPos(VGet(0, 100, 500));
    const int px = (int)at.x, py = (int)at.y;
    for (int order = 0; order < 2; order++) {
        ClearDrawScreen();
        SetUseZBuffer3D(TRUE); SetWriteZBuffer3D(TRUE);
        if (order == 0) { DrawPolygonIndexed3D(farGreen, 4, kRectIdx, 2, 0, TRUE); DrawPolygonIndexed3D(nearRed, 4, kRectIdx, 2, 0, TRUE); }
        else            { DrawPolygonIndexed3D(nearRed, 4, kRectIdx, 2, 0, TRUE); DrawPolygonIndexed3D(farGreen, 4, kRectIdx, 2, 0, TRUE); }
        ScreenFlip();
        CHECK(e.Is(px, py, 1, 0, 0));
    }
    ClearDrawScreen();
    SetUseZBuffer3D(FALSE);
    DrawPolygonIndexed3D(nearRed, 4, kRectIdx, 2, 0, TRUE);
    DrawPolygonIndexed3D(farGreen, 4, kRectIdx, 2, 0, TRUE);
    ScreenFlip();
    CHECK(e.Is(px, py, 0, 1, 0));
}

static void test_3d_near_plane_clipping()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    SetCameraPositionAndTarget_UpVecY(VGet(0, 100, -10), VGet(0, 0, 500));
    VERTEX3D ground[4] = {                // extends behind the camera
        Vx(-300, 0, 3000, 0, 0, 255, 255, 255), Vx(300, 0, 3000, 1, 0, 255, 255, 255),
        Vx(300, 0, -500, 1, 1, 255, 255, 255),  Vx(-300, 0, -500, 0, 1, 255, 255, 255) };
    DrawPolygonIndexed3D(ground, 4, kRectIdx, 2, 0, TRUE);
    ScreenFlip();
    CHECK(e.Is(640, 700, 1, 1, 1));       // near part of the ground reaches the bottom of the screen
    CHECK(e.Is(640, 5, 0, 0, 0));         // sky
}

static void test_3d_texture_uv_on_derived_graph()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255); SetDrawMode(DX_DRAWMODE_NEAREST);
    auto sheet = MakeRaw(4, 2, [](int x, int, uint8_t *p) { p[0] = x < 2 ? 255 : 0; p[1] = x < 2 ? 0 : 255; p[2] = 0; p[3] = 255; });
    std::string path = WriteTemp("sea_div2.raw", sheet);
    int hs[2]; LoadDivGraph(path.c_str(), 2, 2, 1, 2, 2, hs);
    SetCameraPositionAndTarget_UpVecY(VGet(0, 0, -500), VGet(0, 0, 0));
    VERTEX3D q[4]; WallQuad(q, -100, 100, -100, 100, 0, 255, 255, 255);
    DrawPolygonIndexed3D(q, 4, kRectIdx, 2, hs[1], TRUE);               // u in [0,1] must map to the 2nd cell only
    ScreenFlip();
    CHECK(e.Is(640, 360, 0, 1, 0));
    CHECK(e.Is(600, 330, 0, 1, 0));
}

static void test_keys()
{
    Env e;
    char k[256];
    DxSetKeyHeld(KEY_INPUT_A, true);
    DxPulseKey(KEY_INPUT_RETURN);
    GetHitKeyStateAll(k);
    CHECK(k[KEY_INPUT_A] == 1 && k[KEY_INPUT_RETURN] == 1);
    GetHitKeyStateAll(k);
    CHECK(k[KEY_INPUT_A] == 1 && k[KEY_INPUT_RETURN] == 0);              // pulse lasts exactly one read
    DxSetKeyHeld(KEY_INPUT_A, false);
    GetHitKeyStateAll(k); CHECK(k[KEY_INPUT_A] == 0);
}

static void test_handles_and_errors()
{
    Env e;
    CHECK(LoadGraph("/nonexistent/file.png") == -1);
    CHECK(DrawGraph(0, 0, -1, TRUE) == -1);
    CHECK(SetDrawScreen(99999) == -1);
    int g = RawGraph(Rgbw());
    CHECK(SetDrawScreen(g) == -1);                                       // not a render target
    int rt = MakeScreen(8, 8, TRUE);
    CHECK(SetDrawScreen(rt) == 0);
    DeleteGraph(rt);                                                     // deleting the current target falls back to the back buffer
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    DrawBox(0, 0, 10, 10, GetColor(255, 255, 255), TRUE);
    ScreenFlip();
    CHECK(e.Is(5, 5, 1, 1, 1));
    CHECK(DeleteGraph(g) == 0 && DeleteGraph(g) == -1);
    // graph deleted right after being drawn: queued draws must still land
    int g2 = RawGraph(Rgbw());
    SetDrawMode(DX_DRAWMODE_NEAREST);
    DrawRotaGraph3F(300, 300, 0, 0, 10, 10, 0, g2, TRUE);
    DeleteGraph(g2);
    ScreenFlip();
    CHECK(e.Is(305, 305, 1, 0, 0));
}

static void test_batching_reduces_draw_calls()
{
    Env e;
    SetDrawBlendMode(DX_BLENDMODE_ALPHA, 255);
    for (int i = 0; i < 200; i++) DrawBox(i, 0, i + 1, 10, GetColor(255, 255, 255), TRUE);
    ScreenFlip();
    CHECK(e.r.drawCalls == 1);
}

int main()
{
    RUN(test_flat_box);
    RUN(test_no_double_blend_on_quad_diagonal);
    RUN(test_texture_orientation_and_turn);
    RUN(test_rotation_is_clockwise_on_screen);
    RUN(test_rect_rota_pivot_is_rect_relative);
    RUN(test_div_graph_and_refcount);
    RUN(test_render_target_matches_direct_draw);
    RUN(test_render_target_without_alpha_is_opaque);
    RUN(test_blend_add_and_bright);
    RUN(test_shapes);
    RUN(test_camera_projection_matches_reference);
    RUN(test_3d_ground_quad_and_depth);
    RUN(test_3d_near_plane_clipping);
    RUN(test_3d_texture_uv_on_derived_graph);
    RUN(test_keys);
    RUN(test_handles_and_errors);
    RUN(test_batching_reduces_draw_calls);
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
