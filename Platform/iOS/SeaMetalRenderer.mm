// SeaMetalRenderer.mm
//
// NOTE: written without access to Xcode/Metal (see docs/PORTING.md, "Verification status").
// The behaviour it implements is specified by Platform/DxCompat/DxRenderer.h and was verified
// against the CPU reference renderer in Tests/; this file only maps that contract onto Metal.
#import "SeaMetalRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace sea {

namespace {

enum : uint32_t { kFlagTextured = 1, kFlagIgnoreAlpha = 2, kFlagPremultiplied = 4 };

struct FragParams { uint32_t flags; uint32_t pad[3]; };

const MTLPixelFormat kMainFormat   = MTLPixelFormatBGRA8Unorm;   // non-sRGB: D3D9/DxLib blend in gamma space
const MTLPixelFormat kTargetFormat = MTLPixelFormatRGBA8Unorm;
const MTLPixelFormat kDepthFormat  = MTLPixelFormatDepth32Float;

} // namespace

MetalRenderer::MetalRenderer(MTKView *view, int vw, int vh) : view_(view), vw_(vw), vh_(vh)
{
    device_ = view.device;
    queue_ = [device_ newCommandQueue];

    view.colorPixelFormat = kMainFormat;
    view.depthStencilPixelFormat = kDepthFormat;
    view.framebufferOnly = YES;
    view.clearColor = MTLClearColorMake(0, 0, 0, 1);
    view.clearDepth = 1.0;
    view.sampleCount = 1;

    id<MTLLibrary> lib = [device_ newDefaultLibrary];
    vertexFn_ = [lib newFunctionWithName:@"sea_vertex"];
    fragmentFn_ = [lib newFunctionWithName:@"sea_fragment"];
    NSCAssert(vertexFn_ && fragmentFn_, @"SeaShaders.metal is missing from the app target");

    // DxVertex: float3 position @0, float2 uv @12, uchar4 colour @20, stride 24
    vertexDesc_ = [MTLVertexDescriptor vertexDescriptor];
    vertexDesc_.attributes[0].format = MTLVertexFormatFloat3;
    vertexDesc_.attributes[0].offset = offsetof(DxVertex, x);
    vertexDesc_.attributes[0].bufferIndex = 0;
    vertexDesc_.attributes[1].format = MTLVertexFormatFloat2;
    vertexDesc_.attributes[1].offset = offsetof(DxVertex, u);
    vertexDesc_.attributes[1].bufferIndex = 0;
    vertexDesc_.attributes[2].format = MTLVertexFormatUChar4Normalized;
    vertexDesc_.attributes[2].offset = offsetof(DxVertex, r);
    vertexDesc_.attributes[2].bufferIndex = 0;
    vertexDesc_.layouts[0].stride = sizeof(DxVertex);
    vertexDesc_.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    static_assert(sizeof(DxVertex) == 24, "vertex layout assumed by the vertex descriptor");

    MTLDepthStencilDescriptor *dd = [MTLDepthStencilDescriptor new];
    dd.depthCompareFunction = MTLCompareFunctionAlways;
    dd.depthWriteEnabled = NO;
    depthNone_ = [device_ newDepthStencilStateWithDescriptor:dd];
    dd.depthCompareFunction = MTLCompareFunctionLessEqual;
    dd.depthWriteEnabled = YES;
    depthTestWrite_ = [device_ newDepthStencilStateWithDescriptor:dd];
    dd.depthWriteEnabled = NO;
    depthTestOnly_ = [device_ newDepthStencilStateWithDescriptor:dd];

    MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
    sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
    linear_ = [device_ newSamplerStateWithDescriptor:sd];
    sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;
    nearest_ = [device_ newSamplerStateWithDescriptor:sd];

    MTLTextureDescriptor *wd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:kTargetFormat width:1 height:1 mipmapped:NO];
    white_ = [device_ newTextureWithDescriptor:wd];
    const uint8_t px[4] = { 255, 255, 255, 255 };
    [white_ replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:px bytesPerRow:4];
}

MetalRenderer::~MetalRenderer()
{
    EndEncoder();
    if (cmd_) { [cmd_ commit]; cmd_ = nil; }
}

// ------------------------------------------------------------ resources ---
int MetalRenderer::CreateTexture(int w, int h, const uint8_t *rgba, bool renderTarget)
{
    MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:kTargetFormat
                                                                                 width:w height:h mipmapped:NO];
    if (renderTarget) {
        d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        d.storageMode = MTLStorageModePrivate;
    } else {
        d.usage = MTLTextureUsageShaderRead;
        d.storageMode = MTLStorageModeShared;
    }
    id<MTLTexture> t = [device_ newTextureWithDescriptor:d];
    if (!t) return 0;
    if (rgba && !renderTarget)
        [t replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:rgba bytesPerRow:(NSUInteger)w * 4];

    std::lock_guard<std::mutex> lock(mu_);
    const int id = nextId_++;
    TexRec r;
    r.tex = t; r.isTarget = renderTarget; r.needsClear = renderTarget; r.w = w; r.h = h;   // new targets start as transparent black
    textures_[id] = r;
    return id;
}

void MetalRenderer::DestroyTexture(int id)
{
    std::lock_guard<std::mutex> lock(mu_);
    if (curTarget_ == id) EndEncoder();       // (render thread) never leave an encoder open on a dying texture
    textures_.erase(id);                      // in-flight command buffers keep the MTLTexture alive
}

MetalRenderer::TexRec *MetalRenderer::Find(int id)
{
    std::lock_guard<std::mutex> lock(mu_);
    auto it = textures_.find(id);
    return it == textures_.end() ? nullptr : &it->second;   // unordered_map nodes are stable across inserts
}

// ------------------------------------------------------- frame lifecycle ---
void MetalRenderer::BeginFrame()
{
    frameActive_ = true;
    mainNeedsClear_ = true;
    drawable_ = nil;
}

void MetalRenderer::EndFrame()
{
    EndEncoder();
    if (cmd_) { [cmd_ commit]; cmd_ = nil; }
    drawable_ = nil;
    frameActive_ = false;
}

CGRect MetalRenderer::ContentRectInView() const
{
    const CGSize b = view_.bounds.size;
    if (b.width <= 0 || b.height <= 0) return CGRectMake(0, 0, vw_, vh_);
    const CGFloat s = std::min(b.width / vw_, b.height / vh_);
    const CGFloat w = vw_ * s, h = vh_ * s;
    return CGRectMake((b.width - w) * 0.5, (b.height - h) * 0.5, w, h);
}

MTLViewport MetalRenderer::MainViewport() const
{
    const CGSize d = view_.drawableSize;
    const double s = std::min(d.width / vw_, d.height / vh_);
    const double w = vw_ * s, h = vh_ * s;
    return MTLViewport{ std::floor((d.width - w) * 0.5), std::floor((d.height - h) * 0.5), w, h, 0.0, 1.0 };
}

// --------------------------------------------------------------- encoders ---
void MetalRenderer::EndEncoder()
{
    if (enc_) { [enc_ endEncoding]; enc_ = nil; }
    curTarget_ = -1;
}

id<MTLRenderCommandEncoder> MetalRenderer::Encoder(int target)
{
    if (enc_ && curTarget_ == target) return enc_;
    EndEncoder();

    MTLRenderPassDescriptor *rp = nil;
    MTLViewport vp;
    if (target == 0) {
        if (!frameActive_) return nil;
        if (!drawable_) drawable_ = view_.currentDrawable;
        if (!drawable_) return nil;
        rp = [view_.currentRenderPassDescriptor copy];
        if (!rp) return nil;
        rp.colorAttachments[0].loadAction  = mainNeedsClear_ ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 1);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        rp.depthAttachment.loadAction      = mainNeedsClear_ ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.depthAttachment.clearDepth      = 1.0;
        rp.depthAttachment.storeAction     = MTLStoreActionStore;      // later passes in the same frame load it again
        mainNeedsClear_ = false;
        vp = MainViewport();
    } else {
        TexRec *r = Find(target);
        if (!r || !r->isTarget) return nil;
        rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture     = r->tex;
        rp.colorAttachments[0].loadAction  = r->needsClear ? MTLLoadActionClear : MTLLoadActionLoad;
        rp.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 0);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        r->needsClear = false;
        vp = MTLViewport{ 0, 0, (double)r->w, (double)r->h, 0.0, 1.0 };
    }

    if (!cmd_) cmd_ = [queue_ commandBuffer];
    enc_ = [cmd_ renderCommandEncoderWithDescriptor:rp];
    curTarget_ = target;
    [enc_ setViewport:vp];
    [enc_ setCullMode:MTLCullModeNone];
    return enc_;
}

id<MTLRenderPipelineState> MetalRenderer::Pipeline(DxBlend blend, bool isMain)
{
    const int b = (int)blend, m = isMain ? 1 : 0;
    if (pipelines_[b][m]) return pipelines_[b][m];

    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = vertexFn_;
    pd.fragmentFunction = fragmentFn_;
    pd.vertexDescriptor = vertexDesc_;
    pd.colorAttachments[0].pixelFormat = isMain ? kMainFormat : kTargetFormat;
    pd.depthAttachmentPixelFormat = isMain ? kDepthFormat : MTLPixelFormatInvalid;

    MTLRenderPipelineColorAttachmentDescriptor *ca = pd.colorAttachments[0];
    switch (blend) {
        case DxBlend::Opaque:
            ca.blendingEnabled = NO;
            break;
        case DxBlend::Alpha:      // premultiplied: dst = src + dst * (1 - src.a)
            ca.blendingEnabled = YES;
            ca.rgbBlendOperation = ca.alphaBlendOperation = MTLBlendOperationAdd;
            ca.sourceRGBBlendFactor = ca.sourceAlphaBlendFactor = MTLBlendFactorOne;
            ca.destinationRGBBlendFactor = ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            break;
        case DxBlend::Add:        // dst = src + dst
            ca.blendingEnabled = YES;
            ca.rgbBlendOperation = ca.alphaBlendOperation = MTLBlendOperationAdd;
            ca.sourceRGBBlendFactor = ca.sourceAlphaBlendFactor = MTLBlendFactorOne;
            ca.destinationRGBBlendFactor = ca.destinationAlphaBlendFactor = MTLBlendFactorOne;
            break;
    }
    NSError *err = nil;
    id<MTLRenderPipelineState> ps = [device_ newRenderPipelineStateWithDescriptor:pd error:&err];
    NSCAssert(ps, @"pipeline creation failed: %@", err);
    pipelines_[b][m] = ps;
    return ps;
}

// ------------------------------------------------------------------ draw ---
void MetalRenderer::Clear(int target)
{
    if (target == 0) {
        if (!frameActive_) return;
        if (curTarget_ == 0) EndEncoder();      // next main pass starts with a clear
        mainNeedsClear_ = true;
    } else {
        TexRec *r = Find(target);
        if (!r) return;
        if (curTarget_ == target) EndEncoder();
        r->needsClear = true;
    }
}

void MetalRenderer::Draw(const DxDrawState &st, const DxVertex *v, size_t nv, const uint32_t *ix, size_t ni)
{
    if (nv == 0 || ni == 0) return;
    if (st.target == 0 && !frameActive_) return;

    // A render target that was cleared but never drawn to must still read back as cleared.
    TexRec *src = st.texture ? Find(st.texture) : nullptr;
    if (st.texture && !src) return;
    if (src && src->isTarget && src->needsClear) { Encoder(st.texture); EndEncoder(); }

    id<MTLRenderCommandEncoder> enc = Encoder(st.target);
    if (!enc) return;
    const bool isMain = st.target == 0;

    [enc setRenderPipelineState:Pipeline(st.blend, isMain)];
    [enc setDepthStencilState:(isMain && st.depthTest) ? (st.depthWrite ? depthTestWrite_ : depthTestOnly_) : depthNone_];

    const size_t vbytes = nv * sizeof(DxVertex);
    if (vbytes <= 4096) {
        [enc setVertexBytes:v length:vbytes atIndex:0];
    } else {
        id<MTLBuffer> vb = [device_ newBufferWithBytes:v length:vbytes options:MTLResourceStorageModeShared];
        [enc setVertexBuffer:vb offset:0 atIndex:0];
    }
    [enc setVertexBytes:st.matrix length:sizeof(st.matrix) atIndex:1];

    FragParams fp = {};
    if (src) {
        fp.flags |= kFlagTextured;
        if (st.ignoreAlpha) fp.flags |= kFlagIgnoreAlpha;
        if (st.premultiplied) fp.flags |= kFlagPremultiplied;
    }
    [enc setFragmentBytes:&fp length:sizeof(fp) atIndex:0];
    [enc setFragmentTexture:(src ? src->tex : white_) atIndex:0];
    [enc setFragmentSamplerState:(st.linear ? linear_ : nearest_) atIndex:0];

    id<MTLBuffer> ib = [device_ newBufferWithBytes:ix length:ni * sizeof(uint32_t) options:MTLResourceStorageModeShared];
    [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:ni indexType:MTLIndexTypeUInt32
                   indexBuffer:ib indexBufferOffset:0];
}

void MetalRenderer::Present()
{
    EndEncoder();
    if (frameActive_ && drawable_ && cmd_) [cmd_ presentDrawable:drawable_];
    if (cmd_) { [cmd_ commit]; cmd_ = nil; }
    drawable_ = nil;
    frameActive_ = false;            // the drawable is consumed; further main-target draws wait for the next frame
}

} // namespace sea
