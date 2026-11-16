#pragma once
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

static int g_failures = 0, g_checks = 0;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failures++; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, eps) do { g_checks++; double _a = (a), _b = (b); if (std::fabs(_a - _b) > (eps)) { g_failures++; std::printf("  FAIL %s:%d  %s=%g  expected %g (+-%g)\n", __FILE__, __LINE__, #a, _a, _b, (double)(eps)); } } while (0)
#define RUN(fn) do { int before = g_failures; std::printf("[ RUN  ] %s\n", #fn); fn(); std::printf("[ %s ] %s\n", g_failures == before ? " OK " : "FAIL", #fn); } while (0)

// Trivial test image format: int32 w, int32 h, then w*h*4 RGBA bytes.
inline std::vector<uint8_t> MakeRaw(int w, int h, const std::function<void(int, int, uint8_t *)> &fill)
{
    std::vector<uint8_t> d(8 + size_t(w) * h * 4);
    std::memcpy(&d[0], &w, 4); std::memcpy(&d[4], &h, 4);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) fill(x, y, &d[8 + (size_t(y) * w + x) * 4]);
    return d;
}

inline bool DecodeRaw(const uint8_t *data, size_t size, int *w, int *h, std::vector<uint8_t> *rgba)
{
    if (size < 8) return false;
    std::memcpy(w, data, 4); std::memcpy(h, data + 4, 4);
    if (size < 8 + size_t(*w) * *h * 4) return false;
    rgba->assign(data + 8, data + 8 + size_t(*w) * *h * 4);
    return true;
}

inline std::string WriteTemp(const std::string &name, const std::vector<uint8_t> &d)
{
    std::string p = "/tmp/" + name;
    FILE *f = std::fopen(p.c_str(), "wb");
    std::fwrite(d.data(), 1, d.size(), f);
    std::fclose(f);
    return p;
}
