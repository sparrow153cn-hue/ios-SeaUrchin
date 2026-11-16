#pragma once
#include "../DxCompat/DxLib.h"      // sea::DxImageDecoder

// PNG -> libpng (exact straight alpha); everything else (JPEG, GIF, BMP, ...) -> ImageIO.
sea::DxImageDecoder SeaMakeImageDecoder();
