// SeaPng.h - PNG decoding to straight-alpha RGBA8 via libpng's simplified API.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sea {
bool IsPng(const uint8_t *data, size_t size);
// Any PNG colour type / bit depth / palette; output is always RGBA8 with straight alpha.
bool DecodePng(const uint8_t *data, size_t size, int *width, int *height, std::vector<uint8_t> *rgba);
}
