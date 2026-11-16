// DxRenderer.h - what a GPU (or software) backend must provide to DxCore.
//
// All of DxLib's state handling, handle tables, 2D/3D geometry generation and
// batching live in DxCore.cpp and are backend independent (and unit-tested on
// Linux with DxSoftRenderer).  A backend only has to draw triangles.
//
// Conventions (identical for every backend):
//   * Vertices are already in "draw space": 2D draws use virtual pixel
//     coordinates, 3D draws use world coordinates.  `DxDrawState::matrix`
//     (column-major, clip = M * (x,y,z,1)) maps them to clip space.
//   * Clip space is D3D/Metal style: x,y in [-1,1] (+y up), z in [0,1].
//   * Texture row 0 is the TOP row; (u,v)=(0,0) is the top-left texel.
//   * Render-target textures hold PREMULTIPLIED alpha; loaded images hold
//     straight alpha.  The fragment stage must output premultiplied colour:
//         t = sample(texture)             (opaque white if texture == 0)
//         if (ignoreAlpha)            t.a = 1            // rgb used as stored
//         else if (!premultiplied)    t.rgb *= t.a
//         out.rgb = t.rgb * vertex.rgb * vertex.a
//         out.a   = t.a   * vertex.a
//     and blending is expressed in premultiplied terms:
//         Opaque : dst = out
//         Alpha  : dst = out + dst * (1 - out.a)
//         Add    : dst = out + dst
#pragma once

#include <cstdint>
#include <cstddef>

namespace sea {

struct DxVertex {
    float x, y, z;
    float u, v;
    uint8_t r, g, b, a;     // straight tint colour; `a` already includes the draw-blend "pal"
};

enum class DxBlend : uint8_t { Opaque = 0, Alpha, Add };

struct DxDrawState {
    int     target = 0;                 // 0 = main (back buffer); otherwise a texture id created as render target
    int     texture = 0;                // 0 = none (treated as opaque white)
    DxBlend blend = DxBlend::Alpha;
    bool    depthTest = false;          // only ever true for 3D draws
    bool    depthWrite = false;
    bool    linear = true;              // bilinear (true) or nearest sampling
    bool    premultiplied = false;      // sampled texture is premultiplied (render targets)
    bool    ignoreAlpha = false;        // DxLib TransFlag == FALSE (or NOBLEND)
    float   matrix[16] = {};
};

struct DxRenderer {
    virtual ~DxRenderer() {}

    // Thread-safe. `rgba` is straight-alpha RGBA8, w*h*4 bytes; pass nullptr for a render target.
    // Returns an id > 0, or 0 on failure.
    virtual int  CreateTexture(int w, int h, const uint8_t *rgba, bool renderTarget) = 0;
    virtual void DestroyTexture(int id) = 0;

    // Clears the colour (and, for the main target, depth) of a target.
    // Render targets clear to (0,0,0,0); the main target to opaque black. depth -> 1.0.
    virtual void Clear(int target) = 0;

    virtual void Draw(const DxDrawState &state, const DxVertex *vertices, size_t vertexCount,
                      const uint32_t *indices, size_t indexCount) = 0;

    // End of frame: show the main target.
    virtual void Present() = 0;
};

} // namespace sea
