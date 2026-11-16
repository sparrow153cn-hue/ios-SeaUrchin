// SeaMetalRenderer.h - Metal implementation of sea::DxRenderer for an MTKView.
// Objective-C++ only (include from .mm files).
#pragma once

#import <MetalKit/MetalKit.h>
#include "../DxCompat/DxRenderer.h"

#include <mutex>
#include <unordered_map>

namespace sea {

class MetalRenderer : public DxRenderer {
public:
    // The view must already have its device set.  Pixel formats are configured here.
    MetalRenderer(MTKView *view, int virtualWidth, int virtualHeight);
    ~MetalRenderer() override;

    // Call around every drawInMTKView: so that main-target draws are accepted and committed.
    void BeginFrame();
    void EndFrame();

    // Where the virtual (1280x720) screen appears inside the view, in view points (letterboxed, centred).
    CGRect ContentRectInView() const;

    // DxRenderer
    int  CreateTexture(int w, int h, const uint8_t *rgba, bool renderTarget) override;
    void DestroyTexture(int id) override;
    void Clear(int target) override;
    void Draw(const DxDrawState &state, const DxVertex *vertices, size_t vertexCount,
              const uint32_t *indices, size_t indexCount) override;
    void Present() override;

private:
    struct TexRec {
        id<MTLTexture> tex = nil;
        bool isTarget = false;
        bool needsClear = false;
        int w = 0, h = 0;
    };

    TexRec *Find(int id);
    id<MTLRenderCommandEncoder> Encoder(int target);
    void EndEncoder();
    id<MTLRenderPipelineState> Pipeline(DxBlend blend, bool isMain);
    MTLViewport MainViewport() const;

    MTKView *view_ = nil;
    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> queue_ = nil;
    id<MTLFunction> vertexFn_ = nil, fragmentFn_ = nil;
    MTLVertexDescriptor *vertexDesc_ = nil;
    id<MTLRenderPipelineState> pipelines_[3][2] = {};      // [blend][isMain]
    id<MTLDepthStencilState> depthNone_ = nil, depthTestWrite_ = nil, depthTestOnly_ = nil;
    id<MTLSamplerState> linear_ = nil, nearest_ = nil;
    id<MTLTexture> white_ = nil;

    std::mutex mu_;                                        // guards textures_ / nextId_
    std::unordered_map<int, TexRec> textures_;
    int nextId_ = 1;

    // per-frame state (render thread only)
    id<MTLCommandBuffer> cmd_ = nil;
    id<MTLRenderCommandEncoder> enc_ = nil;
    int curTarget_ = -1;
    id<CAMetalDrawable> drawable_ = nil;
    bool frameActive_ = false;
    bool mainNeedsClear_ = true;

    int vw_, vh_;
};

} // namespace sea
