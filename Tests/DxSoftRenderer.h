// DxSoftRenderer.h - slow CPU implementation of sea::DxRenderer.  Test-only:
// it implements the exact fragment/blend contract documented in DxRenderer.h so
// the backend-independent DxCore can be verified without a GPU.
#pragma once

#include "../Platform/DxCompat/DxRenderer.h"

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace sea {

class DxSoftRenderer : public DxRenderer {
public:
    DxSoftRenderer(int mainWidth, int mainHeight);

    int  CreateTexture(int w, int h, const uint8_t *rgba, bool renderTarget) override;
    void DestroyTexture(int id) override;
    void Clear(int target) override;
    void Draw(const DxDrawState &state, const DxVertex *v, size_t nv, const uint32_t *ix, size_t ni) override;
    void Present() override { presentCount++; }

    // Inspection
    struct Pixel { float r, g, b, a; };
    Pixel MainPixel(int x, int y) const;
    Pixel TexPixel(int id, int x, int y) const;
    bool  SaveMainPPM(const std::string &path) const;
    int   presentCount = 0;
    int   drawCalls = 0;
    int   liveTextures() const { return (int)tex_.size(); }

private:
    struct Surface {
        int w = 0, h = 0;
        std::vector<float> c;       // rgba
        std::vector<float> depth;   // main only
        bool isTarget = false;
    };
    Surface main_;
    std::map<int, Surface> tex_;
    int next_ = 1;
    std::mutex mu_;

    Surface *Find(int id);
    struct Frag { float r, g, b, a; };
    Frag Sample(const Surface &t, bool linear, float u, float v) const;
};

} // namespace sea
