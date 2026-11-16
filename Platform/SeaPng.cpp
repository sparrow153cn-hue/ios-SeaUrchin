#include "SeaPng.h"

#include <cstring>
#include <png.h>

namespace sea {

bool IsPng(const uint8_t *data, size_t size)
{
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    return size >= 8 && std::memcmp(data, sig, 8) == 0;
}

bool DecodePng(const uint8_t *data, size_t size, int *width, int *height, std::vector<uint8_t> *rgba)
{
    if (!IsPng(data, size)) return false;
    png_image img;
    std::memset(&img, 0, sizeof(img));
    img.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&img, data, size)) return false;
    img.format = PNG_FORMAT_RGBA;
    rgba->resize(PNG_IMAGE_SIZE(img));
    if (!png_image_finish_read(&img, nullptr, rgba->data(), 0, nullptr)) {
        png_image_free(&img);
        return false;
    }
    *width = static_cast<int>(img.width);
    *height = static_cast<int>(img.height);
    return true;
}

} // namespace sea
