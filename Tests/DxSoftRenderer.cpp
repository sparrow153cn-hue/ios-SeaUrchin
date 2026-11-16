#include "DxSoftRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace sea {

namespace {

struct CV { float x, y, z, w, u, v, r, g, b, a; };      // clip-space vertex with attributes

CV Lerp(const CV &p, const CV &q, float t)
{
    CV o;
    float *po = &o.x; const float *pp = &p.x, *pq = &q.x;
    for (int i = 0; i < 10; i++) po[i] = pp[i] + (pq[i] - pp[i]) * t;
    return o;
}

// Sutherland-Hodgman against one plane; dist(v) >= 0 is inside.
template <class F>
std::vector<CV> ClipPlane(const std::vector<CV> &in, F dist)
{
    std::vector<CV> out;
    for (size_t i = 0; i < in.size(); i++) {
        const CV &a = in[i], &b = in[(i + 1) % in.size()];
        const float da = dist(a), db = dist(b);
        if (da >= 0) out.push_back(a);
        if ((da >= 0) != (db >= 0)) out.push_back(Lerp(a, b, da / (da - db)));
    }
    return out;
}

inline float Edge(float ax, float ay, float bx, float by, float px, float py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}
// Valid when the triangle has been oriented so that Edge() > 0 is "inside".
inline bool TopLeft(float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay;
    return (dy == 0 && dx > 0) || dy < 0;
}

} // namespace

DxSoftRenderer::DxSoftRenderer(int w, int h)
{
    main_.w = w; main_.h = h;
    main_.c.assign(size_t(w) * h * 4, 0.f);
    main_.depth.assign(size_t(w) * h, 1.f);
    Clear(0);
}

int DxSoftRenderer::CreateTexture(int w, int h, const uint8_t *rgba, bool rt)
{
    std::lock_guard<std::mutex> lock(mu_);
    Surface s; s.w = w; s.h = h; s.isTarget = rt;
    s.c.assign(size_t(w) * h * 4, 0.f);
    if (rgba) for (size_t i = 0; i < s.c.size(); i++) s.c[i] = rgba[i] / 255.0f;
    const int id = next_++;
    tex_[id] = std::move(s);
    return id;
}

void DxSoftRenderer::DestroyTexture(int id)
{
    std::lock_guard<std::mutex> lock(mu_);
    tex_.erase(id);
}

DxSoftRenderer::Surface *DxSoftRenderer::Find(int id)
{
    if (id == 0) return &main_;
    auto it = tex_.find(id);
    return it == tex_.end() ? nullptr : &it->second;
}

void DxSoftRenderer::Clear(int target)
{
    std::lock_guard<std::mutex> lock(mu_);
    Surface *s = Find(target);
    if (!s) return;
    for (size_t i = 0; i < s->c.size(); i += 4) {
        s->c[i] = s->c[i + 1] = s->c[i + 2] = 0; s->c[i + 3] = target == 0 ? 1.0f : 0.0f;
    }
    if (target == 0) std::fill(s->depth.begin(), s->depth.end(), 1.0f);
}

DxSoftRenderer::Frag DxSoftRenderer::Sample(const Surface &t, bool linear, float u, float v) const
{
    auto texel = [&](int x, int y) {
        x = std::min(std::max(x, 0), t.w - 1);
        y = std::min(std::max(y, 0), t.h - 1);
        const float *p = &t.c[(size_t(y) * t.w + x) * 4];
        return Frag{ p[0], p[1], p[2], p[3] };
    };
    if (!linear) return texel(int(std::floor(u * t.w)), int(std::floor(v * t.h)));
    const float fx = u * t.w - 0.5f, fy = v * t.h - 0.5f;
    const int x0 = int(std::floor(fx)), y0 = int(std::floor(fy));
    const float tx = fx - x0, ty = fy - y0;
    const Frag a = texel(x0, y0), b = texel(x0 + 1, y0), c = texel(x0, y0 + 1), d = texel(x0 + 1, y0 + 1);
    auto mix = [&](float A, float B, float C, float D) { return (A * (1 - tx) + B * tx) * (1 - ty) + (C * (1 - tx) + D * tx) * ty; };
    return Frag{ mix(a.r, b.r, c.r, d.r), mix(a.g, b.g, c.g, d.g), mix(a.b, b.b, c.b, d.b), mix(a.a, b.a, c.a, d.a) };
}

void DxSoftRenderer::Draw(const DxDrawState &st, const DxVertex *verts, size_t nv, const uint32_t *ix, size_t ni)
{
    std::lock_guard<std::mutex> lock(mu_);
    drawCalls++;
    Surface *dst = Find(st.target);
    if (!dst) return;
    const Surface *tex = st.texture ? Find(st.texture) : nullptr;
    if (st.texture && !tex) return;

    // vertex stage
    std::vector<CV> cv(nv);
    for (size_t i = 0; i < nv; i++) {
        const float in[4] = { verts[i].x, verts[i].y, verts[i].z, 1.0f };
        float c[4];
        for (int row = 0; row < 4; row++) {
            c[row] = 0;
            for (int k = 0; k < 4; k++) c[row] += st.matrix[k * 4 + row] * in[k];
        }
        cv[i] = CV{ c[0], c[1], c[2], c[3], verts[i].u, verts[i].v,
                    verts[i].r / 255.f, verts[i].g / 255.f, verts[i].b / 255.f, verts[i].a / 255.f };
    }

    for (size_t t = 0; t + 2 < ni; t += 3) {
        std::vector<CV> poly = { cv[ix[t]], cv[ix[t + 1]], cv[ix[t + 2]] };
        poly = ClipPlane(poly, [](const CV &v) { return v.z; });            // near  (0 <= z)
        if (poly.size() < 3) continue;
        poly = ClipPlane(poly, [](const CV &v) { return v.w - v.z; });      // far   (z <= w)
        if (poly.size() < 3) continue;

        for (size_t k = 1; k + 1 < poly.size(); k++) {
            CV a = poly[0], b = poly[k], c = poly[k + 1];
            if (a.w <= 0 || b.w <= 0 || c.w <= 0) continue;
            struct S { float x, y, z, iw; } s[3];
            const CV *src[3] = { &a, &b, &c };
            for (int i = 0; i < 3; i++) {
                s[i].iw = 1.0f / src[i]->w;
                s[i].x = (src[i]->x * s[i].iw * 0.5f + 0.5f) * dst->w;
                s[i].y = (0.5f - src[i]->y * s[i].iw * 0.5f) * dst->h;
                s[i].z = src[i]->z * s[i].iw;
            }
            float area = Edge(s[0].x, s[0].y, s[1].x, s[1].y, s[2].x, s[2].y);
            if (area == 0) continue;
            if (area < 0) { std::swap(s[1], s[2]); std::swap(src[1], src[2]); area = -area; }

            const int minx = std::max(0, int(std::floor(std::min({ s[0].x, s[1].x, s[2].x }))));
            const int maxx = std::min(dst->w - 1, int(std::ceil(std::max({ s[0].x, s[1].x, s[2].x }))));
            const int miny = std::max(0, int(std::floor(std::min({ s[0].y, s[1].y, s[2].y }))));
            const int maxy = std::min(dst->h - 1, int(std::ceil(std::max({ s[0].y, s[1].y, s[2].y }))));
            const bool tl01 = TopLeft(s[0].x, s[0].y, s[1].x, s[1].y);
            const bool tl12 = TopLeft(s[1].x, s[1].y, s[2].x, s[2].y);
            const bool tl20 = TopLeft(s[2].x, s[2].y, s[0].x, s[0].y);

            for (int y = miny; y <= maxy; y++)
                for (int x = minx; x <= maxx; x++) {
                    const float px = x + 0.5f, py = y + 0.5f;
                    const float w2 = Edge(s[0].x, s[0].y, s[1].x, s[1].y, px, py);   // weight of vertex 2
                    const float w0 = Edge(s[1].x, s[1].y, s[2].x, s[2].y, px, py);   // weight of vertex 0
                    const float w1 = Edge(s[2].x, s[2].y, s[0].x, s[0].y, px, py);   // weight of vertex 1
                    if (!(w0 > 0 || (w0 == 0 && tl12))) continue;
                    if (!(w1 > 0 || (w1 == 0 && tl20))) continue;
                    if (!(w2 > 0 || (w2 == 0 && tl01))) continue;
                    const float l0 = w0 / area, l1 = w1 / area, l2 = w2 / area;

                    const float z = l0 * s[0].z + l1 * s[1].z + l2 * s[2].z;
                    float *dpx = &dst->c[(size_t(y) * dst->w + x) * 4];
                    if (st.depthTest && !dst->depth.empty()) {
                        float &zb = dst->depth[size_t(y) * dst->w + x];
                        if (z > zb) continue;
                        if (st.depthWrite) zb = z;
                    }
                    // perspective-correct attributes
                    const float p0 = l0 * s[0].iw, p1 = l1 * s[1].iw, p2 = l2 * s[2].iw;
                    const float ps = p0 + p1 + p2;
                    auto attr = [&](float CV::*m) { return (src[0]->*m * p0 + src[1]->*m * p1 + src[2]->*m * p2) / ps; };
                    const float u = attr(&CV::u), v = attr(&CV::v);
                    const float vr = attr(&CV::r), vg = attr(&CV::g), vb = attr(&CV::b), va = attr(&CV::a);

                    Frag f{ 1, 1, 1, 1 };
                    if (tex) {
                        f = Sample(*tex, st.linear, u, v);
                        if (st.ignoreAlpha) f.a = 1;
                        else if (!st.premultiplied) { f.r *= f.a; f.g *= f.a; f.b *= f.a; }
                    }
                    const float orr = f.r * vr * va, og = f.g * vg * va, ob = f.b * vb * va, oa = f.a * va;
                    switch (st.blend) {
                        case DxBlend::Opaque: dpx[0] = orr; dpx[1] = og; dpx[2] = ob; dpx[3] = oa; break;
                        case DxBlend::Alpha:
                            dpx[0] = orr + dpx[0] * (1 - oa); dpx[1] = og + dpx[1] * (1 - oa);
                            dpx[2] = ob + dpx[2] * (1 - oa); dpx[3] = oa + dpx[3] * (1 - oa); break;
                        case DxBlend::Add:
                            dpx[0] += orr; dpx[1] += og; dpx[2] += ob; dpx[3] += oa; break;
                    }
                }
        }
    }
}

DxSoftRenderer::Pixel DxSoftRenderer::MainPixel(int x, int y) const
{
    const float *p = &main_.c[(size_t(y) * main_.w + x) * 4];
    return Pixel{ p[0], p[1], p[2], p[3] };
}

DxSoftRenderer::Pixel DxSoftRenderer::TexPixel(int id, int x, int y) const
{
    const auto it = tex_.find(id);
    const float *p = &it->second.c[(size_t(y) * it->second.w + x) * 4];
    return Pixel{ p[0], p[1], p[2], p[3] };
}

bool DxSoftRenderer::SaveMainPPM(const std::string &path) const
{
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", main_.w, main_.h);
    for (size_t i = 0; i < size_t(main_.w) * main_.h; i++)
        for (int c = 0; c < 3; c++) {
            const float v = std::min(1.0f, std::max(0.0f, main_.c[i * 4 + c]));
            std::fputc(int(v * 255.0f + 0.5f), f);
        }
    std::fclose(f);
    return true;
}

} // namespace sea
