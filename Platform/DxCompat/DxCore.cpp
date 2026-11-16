// DxCore.cpp - backend independent implementation of the DxLib API subset.
//
// Responsibilities: handle tables (images / render targets / derived "div"
// images), DxLib's global draw state (blend, brightness, camera, z-buffer
// flags, current draw screen), conversion of every draw call to triangles, and
// batching of consecutive compatible draws before they reach a DxRenderer.
#include "DxLib.h"
#include "DxRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace sea {
namespace {

// ------------------------------------------------------------------ math ---
struct Mat4 { float m[16]; };           // column-major, m[col*4 + row]

Mat4 Mul(const Mat4 &a, const Mat4 &b)  // a * b
{
    Mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

VECTOR Sub(VECTOR a, VECTOR b) { return VGet(a.x - b.x, a.y - b.y, a.z - b.z); }
VECTOR Cross(VECTOR a, VECTOR b) { return VGet(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
float  Dot(VECTOR a, VECTOR b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
VECTOR Norm(VECTOR a)
{
    const float l = std::sqrt(Dot(a, a));
    return l > 0 ? VGet(a.x / l, a.y / l, a.z / l) : a;
}

// Left-handed look-at (DirectX convention, +Z forward, +Y up), column-vector form.
Mat4 LookAtLH(VECTOR eye, VECTOR target, VECTOR up)
{
    const VECTOR z = Norm(Sub(target, eye));
    const VECTOR x = Norm(Cross(up, z));
    const VECTOR y = Cross(z, x);
    Mat4 r = {};
    r.m[0 * 4 + 0] = x.x; r.m[1 * 4 + 0] = x.y; r.m[2 * 4 + 0] = x.z; r.m[3 * 4 + 0] = -Dot(x, eye);
    r.m[0 * 4 + 1] = y.x; r.m[1 * 4 + 1] = y.y; r.m[2 * 4 + 1] = y.z; r.m[3 * 4 + 1] = -Dot(y, eye);
    r.m[0 * 4 + 2] = z.x; r.m[1 * 4 + 2] = z.y; r.m[2 * 4 + 2] = z.z; r.m[3 * 4 + 2] = -Dot(z, eye);
    r.m[3 * 4 + 3] = 1;
    return r;
}

// Left-handed perspective, depth range [0,1].
Mat4 PerspectiveLH(float fovY, float aspect, float zn, float zf)
{
    const float f = 1.0f / std::tan(fovY * 0.5f);
    Mat4 r = {};
    r.m[0 * 4 + 0] = f / aspect;
    r.m[1 * 4 + 1] = f;
    r.m[2 * 4 + 2] = zf / (zf - zn);
    r.m[3 * 4 + 2] = -zn * zf / (zf - zn);
    r.m[2 * 4 + 3] = 1;
    return r;
}

// Pixel space (origin top-left, +y down) -> clip space.
Mat4 Ortho2D(float w, float h)
{
    Mat4 r = {};
    r.m[0 * 4 + 0] = 2.0f / w;
    r.m[1 * 4 + 1] = -2.0f / h;
    r.m[3 * 4 + 0] = -1.0f;
    r.m[3 * 4 + 1] = 1.0f;
    r.m[3 * 4 + 2] = 0.5f;      // constant depth, never depth tested
    r.m[3 * 4 + 3] = 1.0f;
    return r;
}

const double kPi = 3.14159265358979323846;

// ------------------------------------------------------------- core data ---
struct TexInfo { int w = 0, h = 0; bool isTarget = false; bool hasAlpha = true; int refs = 0; };
struct Graph   { int tex = 0; int x = 0, y = 0, w = 0, h = 0; };

struct Core {
    DxRenderer   *renderer = nullptr;
    DxImageDecoder decoder;
    int vw = 1280, vh = 720;

    std::mutex mu;                                   // guards the two tables
    std::unordered_map<int, Graph>   graphs;
    std::unordered_map<int, TexInfo> textures;
    int nextHandle = 1;

    // draw state
    int   blendMode = DX_BLENDMODE_NOBLEND, pal = 255;
    int   bright[3] = { 255, 255, 255 };
    int   drawMode = DX_DRAWMODE_BILINEAR;
    bool  useZ = false, writeZ = false;
    int   target = DX_SCREEN_BACK;

    // camera
    VECTOR eye, look, up = { 0, 1, 0 };
    float fovDeg = 60.0f, nearClip = 10.0f, farClip = 20000.0f;

    // batch
    DxDrawState cur;
    bool curValid = false;
    std::vector<DxVertex> verts;
    std::vector<uint32_t> idx;

    // keys
    std::mutex keyMu;
    bool held[256] = {};
    bool pulse[256] = {};
    int  nextKeyInput = 1;

    void ResetCamera()
    {
        // DxLib's default: the z=0 plane maps 1:1 to the screen.
        const float t = std::tan(fovDeg * float(kPi) / 360.0f);
        eye  = VGet(vw * 0.5f, vh * 0.5f, -(vh * 0.5f) / t);
        look = VGet(vw * 0.5f, vh * 0.5f, 0.0f);
        up   = VGet(0, 1, 0);
    }

    // ----------------------------------------------------------- batching
    static bool SameState(const DxDrawState &a, const DxDrawState &b)
    {
        return a.target == b.target && a.texture == b.texture && a.blend == b.blend &&
               a.depthTest == b.depthTest && a.depthWrite == b.depthWrite && a.linear == b.linear &&
               a.premultiplied == b.premultiplied && a.ignoreAlpha == b.ignoreAlpha &&
               std::memcmp(a.matrix, b.matrix, sizeof(a.matrix)) == 0;
    }

    void Flush()
    {
        if (!verts.empty() && renderer)
            renderer->Draw(cur, verts.data(), verts.size(), idx.data(), idx.size());
        verts.clear();
        idx.clear();
    }

    void Submit(const DxDrawState &st, const DxVertex *v, size_t nv, const uint32_t *ix, size_t ni)
    {
        if (!curValid || !SameState(cur, st)) { Flush(); cur = st; curValid = true; }
        const uint32_t base = static_cast<uint32_t>(verts.size());
        verts.insert(verts.end(), v, v + nv);
        for (size_t i = 0; i < ni; i++) idx.push_back(base + ix[i]);
        if (verts.size() > 50000) Flush();
    }

    // ------------------------------------------------------- state helpers
    bool TargetInfo(int *tex, int *w, int *h)
    {
        if (target == DX_SCREEN_BACK) { *tex = 0; *w = vw; *h = vh; return true; }
        std::lock_guard<std::mutex> lock(mu);
        auto g = graphs.find(target);
        if (g == graphs.end()) return false;
        *tex = g->second.tex; *w = g->second.w; *h = g->second.h;
        return true;
    }

    // Builds the draw state for the current globals. `graph` may be <= 0 (untextured).
    bool MakeState(DxDrawState *st, bool is3D, int graph, bool trans, Graph *outGraph, TexInfo *outTex)
    {
        int ttex, tw, th;
        if (!TargetInfo(&ttex, &tw, &th)) return false;

        *st = DxDrawState();
        st->target = ttex;
        st->blend = blendMode == DX_BLENDMODE_NOBLEND ? DxBlend::Opaque :
                    blendMode == DX_BLENDMODE_ADD     ? DxBlend::Add : DxBlend::Alpha;
        st->linear = drawMode == DX_DRAWMODE_BILINEAR;
        st->ignoreAlpha = !trans || blendMode == DX_BLENDMODE_NOBLEND;

        if (graph > 0) {
            std::lock_guard<std::mutex> lock(mu);
            auto g = graphs.find(graph);
            if (g == graphs.end()) return false;
            auto t = textures.find(g->second.tex);
            if (t == textures.end()) return false;
            *outGraph = g->second;
            *outTex = t->second;
            st->texture = g->second.tex;
            st->premultiplied = t->second.isTarget;
            if (t->second.isTarget && !t->second.hasAlpha) st->ignoreAlpha = true;
        }

        // Depth only exists on the main target.
        st->depthTest  = is3D && useZ && ttex == 0;
        st->depthWrite = st->depthTest && writeZ;

        Mat4 m;
        if (is3D) {
            const Mat4 view = LookAtLH(eye, look, up);
            const Mat4 proj = PerspectiveLH(fovDeg * float(kPi) / 180.0f, float(tw) / float(th), nearClip, farClip);
            m = Mul(proj, view);
        } else {
            m = Ortho2D(float(tw), float(th));
        }
        std::memcpy(st->matrix, m.m, sizeof(st->matrix));
        return true;
    }

    uint8_t PalAlpha() const
    {
        return blendMode == DX_BLENDMODE_NOBLEND ? 255 : static_cast<uint8_t>(std::min(255, std::max(0, pal)));
    }

    DxVertex V2(float x, float y, float u, float v, uint8_t r, uint8_t g, uint8_t b, uint8_t a) const
    {
        DxVertex d; d.x = x; d.y = y; d.z = 0; d.u = u; d.v = v; d.r = r; d.g = g; d.b = b; d.a = a;
        return d;
    }

    // Colour for flat shapes: 0x00RRGGBB scaled by the draw brightness.
    void ShapeColor(unsigned int c, uint8_t *r, uint8_t *g, uint8_t *b) const
    {
        *r = static_cast<uint8_t>(((c >> 16) & 255) * bright[0] / 255);
        *g = static_cast<uint8_t>(((c >> 8) & 255) * bright[1] / 255);
        *b = static_cast<uint8_t>((c & 255) * bright[2] / 255);
    }

    // Textured quad; corners in order TL, TR, BR, BL; (u0,v0)-(u1,v1) selects the source area
    // in *rect-relative pixels* of the graph.
    int TexturedQuad(const float px[4], const float py[4], int graph, bool trans,
                     int srcX, int srcY, int srcW, int srcH, bool turn)
    {
        DxDrawState st; Graph g; TexInfo t;
        if (graph <= 0 || !MakeState(&st, false, graph, trans, &g, &t)) return -1;
        float u0 = float(g.x + srcX) / t.w, u1 = float(g.x + srcX + srcW) / t.w;
        const float v0 = float(g.y + srcY) / t.h, v1 = float(g.y + srcY + srcH) / t.h;
        if (turn) std::swap(u0, u1);
        const uint8_t a = PalAlpha();
        const uint8_t r = uint8_t(bright[0]), gg = uint8_t(bright[1]), b = uint8_t(bright[2]);
        const DxVertex v[4] = {
            V2(px[0], py[0], u0, v0, r, gg, b, a), V2(px[1], py[1], u1, v0, r, gg, b, a),
            V2(px[2], py[2], u1, v1, r, gg, b, a), V2(px[3], py[3], u0, v1, r, gg, b, a) };
        static const uint32_t ix[6] = { 0, 1, 2, 0, 2, 3 };
        Submit(st, v, 4, ix, 6);
        return 0;
    }

    int SolidTriangles(const float *xy, int vertexCount, const uint32_t *ix, int indexCount, unsigned int color)
    {
        DxDrawState st; Graph g; TexInfo t;
        if (!MakeState(&st, false, 0, true, &g, &t)) return -1;
        uint8_t r, gg, b; ShapeColor(color, &r, &gg, &b);
        const uint8_t a = PalAlpha();
        std::vector<DxVertex> v(vertexCount);
        for (int i = 0; i < vertexCount; i++) v[i] = V2(xy[i * 2], xy[i * 2 + 1], 0, 0, r, gg, b, a);
        Submit(st, v.data(), v.size(), ix, indexCount);
        return 0;
    }

    int SolidQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, unsigned int color)
    {
        const float xy[8] = { x1, y1, x2, y2, x3, y3, x4, y4 };
        static const uint32_t ix[6] = { 0, 1, 2, 0, 2, 3 };
        return SolidTriangles(xy, 4, ix, 6, color);
    }

    // A thick line segment as a quad.
    int ThickLine(float x1, float y1, float x2, float y2, float thickness, unsigned int color)
    {
        const float dx = x2 - x1, dy = y2 - y1;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len <= 0.0f) return 0;
        const float h = std::max(thickness, 0.0f) * 0.5f;
        const float nx = -dy / len * h, ny = dx / len * h;
        return SolidQuad(x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny, color);
    }

    int AddTexture(int w, int h, const uint8_t *rgba, bool isTarget, bool hasAlpha)
    {
        if (!renderer || w <= 0 || h <= 0 || w > 16384 || h > 16384) return -1;
        const int tex = renderer->CreateTexture(w, h, rgba, isTarget);
        if (tex <= 0) return -1;
        std::lock_guard<std::mutex> lock(mu);
        TexInfo ti; ti.w = w; ti.h = h; ti.isTarget = isTarget; ti.hasAlpha = hasAlpha; ti.refs = 1;
        textures[tex] = ti;
        Graph g; g.tex = tex; g.x = 0; g.y = 0; g.w = w; g.h = h;
        const int handle = nextHandle++;
        graphs[handle] = g;
        return handle;
    }

    bool ReadFile(const char *path, std::vector<uint8_t> *out)
    {
        FILE *f = std::fopen(path, "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (size <= 0) { std::fclose(f); return false; }
        out->resize(static_cast<size_t>(size));
        const size_t got = std::fread(out->data(), 1, out->size(), f);
        std::fclose(f);
        return got == out->size();
    }
};

Core *g_core = nullptr;

inline bool Ready() { return g_core && g_core->renderer; }

} // namespace

// ================================================== integration (host) API ==
void DxInitialize(DxRenderer *renderer, int virtualWidth, int virtualHeight, DxImageDecoder decoder)
{
    delete g_core;
    g_core = new Core();
    g_core->renderer = renderer;
    g_core->vw = virtualWidth;
    g_core->vh = virtualHeight;
    g_core->decoder = std::move(decoder);
    g_core->ResetCamera();
}

void DxShutdown()
{
    delete g_core;
    g_core = nullptr;
}

void DxSetProjection(float verticalFovDegrees, float nearClip, float farClip)
{
    if (!g_core) return;
    g_core->fovDeg = verticalFovDegrees;
    g_core->nearClip = nearClip;
    g_core->farClip = farClip;
}

void DxSetKeyHeld(int code, bool held)
{
    if (!g_core || code < 0 || code > 255) return;
    std::lock_guard<std::mutex> lock(g_core->keyMu);
    g_core->held[code] = held;
}

void DxPulseKey(int code)
{
    if (!g_core || code < 0 || code > 255) return;
    std::lock_guard<std::mutex> lock(g_core->keyMu);
    g_core->pulse[code] = true;
}

} // namespace sea

// ============================================================ DxLib API ====
using namespace sea;

// ------------------------------------------------------------ resources ---
int CreateGraphFromMem(const void *data, int size, const void *, int, int, int)
{
    if (!Ready() || !data || size <= 0 || !g_core->decoder) return -1;
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    if (!g_core->decoder(static_cast<const uint8_t *>(data), static_cast<size_t>(size), &w, &h, &rgba)) return -1;
    if (rgba.size() < size_t(w) * size_t(h) * 4) return -1;
    return g_core->AddTexture(w, h, rgba.data(), false, true);
}

int LoadGraph(const char *file, int)
{
    if (!Ready() || !file) return -1;
    std::vector<uint8_t> bytes;
    if (!g_core->ReadFile(file, &bytes)) return -1;
    return CreateGraphFromMem(bytes.data(), static_cast<int>(bytes.size()), nullptr, 0, TRUE, FALSE);
}

int LoadDivGraph(const char *file, int allNum, int xNum, int yNum, int xSize, int ySize, int *handleBuf, int)
{
    if (!Ready() || !file || !handleBuf || allNum <= 0 || xNum <= 0 || yNum <= 0 || xSize <= 0 || ySize <= 0) return -1;
    std::vector<uint8_t> bytes;
    if (!g_core->ReadFile(file, &bytes) || !g_core->decoder) return -1;
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    if (!g_core->decoder(bytes.data(), bytes.size(), &w, &h, &rgba)) return -1;
    if (xNum * xSize > w || yNum * ySize > h) return -1;     // DxLib fails for out-of-bounds layouts too
    const int base = g_core->AddTexture(w, h, rgba.data(), false, true);
    if (base < 0) return -1;

    std::lock_guard<std::mutex> lock(g_core->mu);
    const Graph whole = g_core->graphs[base];
    g_core->graphs.erase(base);
    g_core->textures[whole.tex].refs = 0;
    for (int i = 0; i < allNum; i++) {
        Graph g; g.tex = whole.tex; g.w = xSize; g.h = ySize;
        g.x = (i % xNum) * xSize;
        g.y = (i / xNum) * ySize;
        const int handle = g_core->nextHandle++;
        g_core->graphs[handle] = g;
        g_core->textures[whole.tex].refs++;
        handleBuf[i] = handle;
    }
    return 0;
}

int MakeScreen(int w, int h, int useAlpha)
{
    if (!Ready()) return -1;
    return g_core->AddTexture(w, h, nullptr, true, useAlpha != 0);
}

int DeleteGraph(int handle, int)
{
    if (!Ready()) return -1;
    g_core->Flush();                       // pending draws may still reference it
    int destroy = 0;
    {
        std::lock_guard<std::mutex> lock(g_core->mu);
        auto g = g_core->graphs.find(handle);
        if (g == g_core->graphs.end()) return -1;
        const int tex = g->second.tex;
        g_core->graphs.erase(g);
        auto t = g_core->textures.find(tex);
        if (t != g_core->textures.end() && --t->second.refs <= 0) { g_core->textures.erase(t); destroy = tex; }
    }
    if (g_core->target == handle) g_core->target = DX_SCREEN_BACK;
    if (destroy) g_core->renderer->DestroyTexture(destroy);
    return 0;
}

int GetGraphSize(int handle, int *w, int *h)
{
    if (!Ready()) return -1;
    std::lock_guard<std::mutex> lock(g_core->mu);
    auto g = g_core->graphs.find(handle);
    if (g == g_core->graphs.end()) return -1;
    if (w) *w = g->second.w;
    if (h) *h = g->second.h;
    return 0;
}

// ---------------------------------------------------------- frame / state ---
int SetDrawScreen(int screen)
{
    if (!Ready()) return -1;
    if (screen != DX_SCREEN_BACK) {
        std::lock_guard<std::mutex> lock(g_core->mu);
        auto g = g_core->graphs.find(screen);
        if (g == g_core->graphs.end()) return -1;
        auto t = g_core->textures.find(g->second.tex);
        if (t == g_core->textures.end() || !t->second.isTarget) return -1;
    }
    if (g_core->target != screen) g_core->Flush();
    g_core->target = screen;
    return 0;
}

int ClearDrawScreen(void *)
{
    if (!Ready()) return -1;
    int tex, w, h;
    if (!g_core->TargetInfo(&tex, &w, &h)) return -1;
    g_core->Flush();
    g_core->renderer->Clear(tex);
    return 0;
}

int ScreenFlip(void)
{
    if (!Ready()) return -1;
    g_core->Flush();
    g_core->renderer->Present();
    return 0;
}

int SetDrawBlendMode(int mode, int pal)
{
    if (!Ready()) return -1;
    g_core->blendMode = mode;
    g_core->pal = pal;
    return 0;
}

int SetDrawBright(int r, int g, int b)
{
    if (!Ready()) return -1;
    g_core->bright[0] = std::min(255, std::max(0, r));
    g_core->bright[1] = std::min(255, std::max(0, g));
    g_core->bright[2] = std::min(255, std::max(0, b));
    return 0;
}

int SetDrawMode(int mode) { if (!Ready()) return -1; g_core->drawMode = mode; return 0; }
int SetUseZBuffer3D(int f) { if (!Ready()) return -1; g_core->useZ = f != 0; return 0; }
int SetWriteZBuffer3D(int f) { if (!Ready()) return -1; g_core->writeZ = f != 0; return 0; }
int SetUseLighting(int) { return 0; }                 // nothing is ever lit
int SetUseASyncLoadFlag(int) { return 0; }

// ------------------------------------------------------------ 2D drawing ---
int DrawGraphF(float x, float y, int h, int trans)
{
    if (!Ready()) return -1;
    int w, hh;
    if (GetGraphSize(h, &w, &hh) < 0) return -1;
    const float px[4] = { x, x + w, x + w, x }, py[4] = { y, y, y + hh, y + hh };
    return g_core->TexturedQuad(px, py, h, trans != 0, 0, 0, w, hh, false);
}

int DrawGraph(int x, int y, int h, int trans) { return DrawGraphF(float(x), float(y), h, trans); }

int DrawRectGraph(int dx, int dy, int sx, int sy, int w, int h, int handle, int trans, int turn)
{
    if (!Ready()) return -1;
    const float px[4] = { float(dx), float(dx + w), float(dx + w), float(dx) };
    const float py[4] = { float(dy), float(dy), float(dy + h), float(dy + h) };
    return g_core->TexturedQuad(px, py, handle, trans != 0, sx, sy, w, h, turn != 0);
}

int DrawRectRotaGraph3F(float x, float y, int sx, int sy, int w, int h, float cx, float cy,
                        double ex, double ey, double angle, int handle, int trans, int turn)
{
    if (!Ready()) return -1;
    const double c = std::cos(angle), s = std::sin(angle);
    const double lx[4] = { 0, double(w), double(w), 0 }, ly[4] = { 0, 0, double(h), double(h) };
    float px[4], py[4];
    for (int i = 0; i < 4; i++) {
        const double dx = (lx[i] - cx) * ex, dy = (ly[i] - cy) * ey;
        px[i] = float(x + dx * c - dy * s);
        py[i] = float(y + dx * s + dy * c);
    }
    return g_core->TexturedQuad(px, py, handle, trans != 0, sx, sy, w, h, turn != 0);
}

int DrawRotaGraph3F(float x, float y, float cx, float cy, double ex, double ey, double angle,
                    int handle, int trans, int turn)
{
    int w, h;
    if (GetGraphSize(handle, &w, &h) < 0) return -1;
    return DrawRectRotaGraph3F(x, y, 0, 0, w, h, cx, cy, ex, ey, angle, handle, trans, turn);
}

int DrawRectModiGraphF(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
                       int sx, int sy, int sw, int sh, int handle, int trans)
{
    if (!Ready()) return -1;
    const float px[4] = { x1, x2, x3, x4 }, py[4] = { y1, y2, y3, y4 };
    return g_core->TexturedQuad(px, py, handle, trans != 0, sx, sy, sw, sh, false);
}

int DrawModiGraphF(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
                   int handle, int trans)
{
    int w, h;
    if (GetGraphSize(handle, &w, &h) < 0) return -1;
    return DrawRectModiGraphF(x1, y1, x2, y2, x3, y3, x4, y4, 0, 0, w, h, handle, trans);
}

int DrawPixel(int x, int y, unsigned int color)
{
    if (!Ready()) return -1;
    return g_core->SolidQuad(float(x), float(y), float(x + 1), float(y), float(x + 1), float(y + 1), float(x), float(y + 1), color);
}

int DrawBox(int x1, int y1, int x2, int y2, unsigned int color, int fill)
{
    if (!Ready()) return -1;
    if (fill) return g_core->SolidQuad(float(x1), float(y1), float(x2), float(y1), float(x2), float(y2), float(x1), float(y2), color);
    // 1px outline inside the rectangle
    const float l = float(x1), t = float(y1), r = float(x2), b = float(y2);
    g_core->SolidQuad(l, t, r, t, r, t + 1, l, t + 1, color);
    g_core->SolidQuad(l, b - 1, r, b - 1, r, b, l, b, color);
    g_core->SolidQuad(l, t + 1, l + 1, t + 1, l + 1, b - 1, l, b - 1, color);
    return g_core->SolidQuad(r - 1, t + 1, r, t + 1, r, b - 1, r - 1, b - 1, color);
}

int DrawBoxAA(float x1, float y1, float x2, float y2, unsigned int color, int fill, float thickness)
{
    if (!Ready()) return -1;
    if (fill) return g_core->SolidQuad(x1, y1, x2, y1, x2, y2, x1, y2, color);
    g_core->ThickLine(x1 - thickness * 0.5f, y1, x2 + thickness * 0.5f, y1, thickness, color);
    g_core->ThickLine(x1 - thickness * 0.5f, y2, x2 + thickness * 0.5f, y2, thickness, color);
    g_core->ThickLine(x1, y1, x1, y2, thickness, color);
    return g_core->ThickLine(x2, y1, x2, y2, thickness, color);
}

int DrawLineAA(float x1, float y1, float x2, float y2, unsigned int color, float thickness)
{
    if (!Ready()) return -1;
    return g_core->ThickLine(x1, y1, x2, y2, thickness, color);
}

int DrawOvalAA(float cx, float cy, float rx, float ry, int posnum, unsigned int color, int fill, float thickness)
{
    if (!Ready() || posnum < 3) return -1;
    posnum = std::min(posnum, 1024);
    std::vector<float> xy;
    std::vector<uint32_t> ix;
    if (fill) {
        xy.push_back(cx); xy.push_back(cy);
        for (int i = 0; i < posnum; i++) {
            const double a = 2.0 * kPi * i / posnum;
            xy.push_back(cx + float(std::cos(a)) * rx);
            xy.push_back(cy + float(std::sin(a)) * ry);
        }
        for (int i = 0; i < posnum; i++) { ix.push_back(0); ix.push_back(1 + i); ix.push_back(1 + (i + 1) % posnum); }
    } else {
        const float h = thickness * 0.5f;
        for (int i = 0; i < posnum; i++) {
            const double a = 2.0 * kPi * i / posnum;
            const float c = float(std::cos(a)), s = float(std::sin(a));
            xy.push_back(cx + c * (rx + h)); xy.push_back(cy + s * (ry + h));       // outer
            xy.push_back(cx + c * std::max(rx - h, 0.0f)); xy.push_back(cy + s * std::max(ry - h, 0.0f)); // inner
        }
        for (int i = 0; i < posnum; i++) {
            const uint32_t o0 = 2 * i, i0 = 2 * i + 1, o1 = 2 * ((i + 1) % posnum), i1 = 2 * ((i + 1) % posnum) + 1;
            ix.push_back(o0); ix.push_back(i0); ix.push_back(o1);
            ix.push_back(o1); ix.push_back(i0); ix.push_back(i1);
        }
    }
    return g_core->SolidTriangles(xy.data(), int(xy.size() / 2), ix.data(), int(ix.size()), color);
}

// ------------------------------------------------------------------- 3D ---
int SetCameraPositionAndTarget_UpVecY(VECTOR pos, VECTOR target)
{
    if (!Ready()) return -1;
    g_core->eye = pos;
    g_core->look = target;
    g_core->up = VGet(0, 1, 0);
    return 0;
}

VECTOR ConvWorldPosToScreenPos(VECTOR p)
{
    if (!Ready()) return VGet(0, 0, 0);
    int tex, w, h;
    if (!g_core->TargetInfo(&tex, &w, &h)) { w = g_core->vw; h = g_core->vh; }
    const Mat4 view = LookAtLH(g_core->eye, g_core->look, g_core->up);
    const Mat4 proj = PerspectiveLH(g_core->fovDeg * float(kPi) / 180.0f, float(w) / float(h), g_core->nearClip, g_core->farClip);
    const Mat4 m = Mul(proj, view);
    float c[4];
    const float in[4] = { p.x, p.y, p.z, 1.0f };
    for (int row = 0; row < 4; row++) {
        c[row] = 0;
        for (int k = 0; k < 4; k++) c[row] += m.m[k * 4 + row] * in[k];
    }
    if (c[3] <= 0.0f) return VGet(0, 0, -1.0f);            // behind the camera
    const float nx = c[0] / c[3], ny = c[1] / c[3], nz = c[2] / c[3];
    return VGet((nx * 0.5f + 0.5f) * w, (0.5f - ny * 0.5f) * h, nz);
}

int DrawPolygonIndexed3D(const VERTEX3D *vtx, int vertexNum, const unsigned short *indices,
                         int polygonNum, int handle, int trans)
{
    if (!Ready() || !vtx || !indices || vertexNum <= 0 || polygonNum <= 0) return -1;
    DxDrawState st; Graph g; TexInfo t;
    if (!g_core->MakeState(&st, true, handle, trans != 0, &g, &t)) return -1;

    const bool textured = st.texture != 0;
    const uint8_t pal = g_core->PalAlpha();
    std::vector<DxVertex> v(vertexNum);
    for (int i = 0; i < vertexNum; i++) {
        DxVertex &d = v[i];
        d.x = vtx[i].pos.x; d.y = vtx[i].pos.y; d.z = vtx[i].pos.z;
        if (textured) {
            d.u = (g.x + vtx[i].u * g.w) / float(t.w);
            d.v = (g.y + vtx[i].v * g.h) / float(t.h);
        } else d.u = d.v = 0;
        d.r = uint8_t(vtx[i].dif.r * g_core->bright[0] / 255);
        d.g = uint8_t(vtx[i].dif.g * g_core->bright[1] / 255);
        d.b = uint8_t(vtx[i].dif.b * g_core->bright[2] / 255);
        d.a = uint8_t(vtx[i].dif.a * pal / 255);
    }
    std::vector<uint32_t> ix(size_t(polygonNum) * 3);
    for (size_t i = 0; i < ix.size(); i++) {
        if (indices[i] >= vertexNum) return -1;
        ix[i] = indices[i];
    }
    g_core->Submit(st, v.data(), v.size(), ix.data(), ix.size());
    return 0;
}

int DrawTriangle3D(VECTOR p1, VECTOR p2, VECTOR p3, unsigned int color, int fill)
{
    if (!Ready()) return -1;
    if (!fill) return 0;                                    // outline triangles are not used by Seaurchin
    VERTEX3D v[3];
    const VECTOR ps[3] = { p1, p2, p3 };
    for (int i = 0; i < 3; i++) {
        v[i].pos = ps[i];
        v[i].norm = VGet(0, 0, -1);
        v[i].dif = GetColorU8((color >> 16) & 255, (color >> 8) & 255, color & 255, 255);
        v[i].spc = GetColorU8(0, 0, 0, 0);
        v[i].u = v[i].v = v[i].su = v[i].sv = 0;
    }
    static const unsigned short ix[3] = { 0, 1, 2 };
    return DrawPolygonIndexed3D(v, 3, ix, 1, 0, TRUE);
}

// ------------------------------------------------------------ input / misc ---
int GetHitKeyStateAll(char *buf)
{
    if (!g_core || !buf) return -1;
    std::lock_guard<std::mutex> lock(g_core->keyMu);
    for (int i = 0; i < 256; i++) buf[i] = (g_core->held[i] || g_core->pulse[i]) ? 1 : 0;
    std::memset(g_core->pulse, 0, sizeof(g_core->pulse));
    return 0;
}

int CheckHitKey(int code)
{
    if (!g_core || code < 0 || code > 255) return -1;
    std::lock_guard<std::mutex> lock(g_core->keyMu);
    return (g_core->held[code] || g_core->pulse[code]) ? 1 : 0;
}

int MakeKeyInput(int, int, int, int, int, int) { return g_core ? g_core->nextKeyInput++ : -1; }
int DeleteKeyInput(int) { return 0; }
int SetActiveKeyInput(int) { return 0; }
int CheckKeyInput(int) { return 0; }            // 0 = still editing
int GetKeyInputString(char *buf, int) { if (buf) buf[0] = 0; return 0; }
int GetKeyInputCursorPosition(int) { return 0; }
int GetKeyInputSelectArea(int *s, int *e, int) { if (s) *s = -1; if (e) *e = -1; return 0; }
