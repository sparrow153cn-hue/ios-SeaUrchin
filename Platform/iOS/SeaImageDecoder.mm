#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include "SeaImageDecoder.h"
#include "../SeaPng.h"

static bool DecodeWithImageIO(const uint8_t *data, size_t size, int *w, int *h, std::vector<uint8_t> *rgba)
{
    CFDataRef cfData = CFDataCreateWithBytesNoCopy(NULL, data, (CFIndex)size, kCFAllocatorNull);
    if (!cfData) return false;
    CGImageSourceRef source = CGImageSourceCreateWithData(cfData, NULL);
    CGImageRef image = source ? CGImageSourceCreateImageAtIndex(source, 0, NULL) : NULL;
    bool ok = false;
    if (image) {
        const size_t iw = CGImageGetWidth(image), ih = CGImageGetHeight(image);
        if (iw > 0 && ih > 0) {
            rgba->assign(iw * ih * 4, 0);
            CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
            // CGBitmapContext cannot produce straight alpha, so decode premultiplied and convert back.
            CGContextRef ctx = CGBitmapContextCreate(rgba->data(), iw, ih, 8, iw * 4, cs,
                                                     kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
            if (ctx) {
                CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)iw, (CGFloat)ih), image);
                uint8_t *p = rgba->data();
                for (size_t i = 0; i < iw * ih; i++, p += 4) {
                    const unsigned a = p[3];
                    if (a != 0 && a != 255)
                        for (int c = 0; c < 3; c++) p[c] = (uint8_t)std::min(255u, (p[c] * 255u + a / 2) / a);
                }
                *w = (int)iw; *h = (int)ih;
                ok = true;
                CGContextRelease(ctx);
            }
            CGColorSpaceRelease(cs);
        }
        CGImageRelease(image);
    }
    if (source) CFRelease(source);
    CFRelease(cfData);
    return ok;
}

sea::DxImageDecoder SeaMakeImageDecoder()
{
    return [](const uint8_t *data, size_t size, int *w, int *h, std::vector<uint8_t> *rgba) -> bool {
        if (sea::IsPng(data, size) && sea::DecodePng(data, size, w, h, rgba)) return true;
        return DecodeWithImageIO(data, size, w, h, rgba);
    };
}
