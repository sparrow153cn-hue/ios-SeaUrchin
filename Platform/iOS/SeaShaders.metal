// SeaShaders.metal
// Implements the fragment/blend contract documented in Platform/DxCompat/DxRenderer.h.
// Keep in sync with Tests/DxSoftRenderer.cpp, which is the tested reference.
#include <metal_stdlib>
using namespace metal;

struct SeaVertexIn {
    float3 position [[attribute(0)]];
    float2 uv       [[attribute(1)]];
    float4 color    [[attribute(2)]];   // uchar4 normalised -> 0..1 (tint, .a already includes draw "pal")
};

struct SeaVertexOut {
    float4 position [[position]];
    float2 uv;
    float4 color;
};

struct SeaFragParams {
    uint flags;                          // bit0: textured, bit1: ignore texture alpha, bit2: texture is premultiplied
    uint pad0, pad1, pad2;
};

vertex SeaVertexOut sea_vertex(SeaVertexIn in [[stage_in]],
                               constant float4x4 &mvp [[buffer(1)]])
{
    SeaVertexOut out;
    out.position = mvp * float4(in.position, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
}

fragment float4 sea_fragment(SeaVertexOut in [[stage_in]],
                             constant SeaFragParams &p [[buffer(0)]],
                             texture2d<float> tex [[texture(0)]],
                             sampler smp [[sampler(0)]])
{
    float4 t = float4(1.0);
    if (p.flags & 1u) {
        t = tex.sample(smp, in.uv);
        if (p.flags & 2u) {
            t.a = 1.0;                   // DxLib TransFlag == FALSE / NOBLEND: rgb used as stored
        } else if (!(p.flags & 4u)) {
            t.rgb *= t.a;                // straight-alpha image -> premultiplied
        }
    }
    const float4 c = in.color;
    // premultiplied output; every blend state is expressed in premultiplied terms
    return float4(t.rgb * c.rgb * c.a, t.a * c.a);
}
