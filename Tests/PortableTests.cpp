// Tests for the platform-independent pieces of the port, including real game sources
// (Controller.cpp, Font.h) and real skin data (PNG images, .sif fonts).
#include "TestUtil.h"
#include "DxSoftRenderer.h"
#include "../Platform/SeaPlatform.h"
#include "../Platform/SeaPng.h"
#include "../Platform/SeaKeyMap.h"
#include "../Platform/SeaTouchRouter.h"
#include "../Seaurchin/Controller.h"
#include "../Seaurchin/Font.h"

#include <fstream>
#include <iterator>

using namespace sea;

static std::vector<uint8_t> Slurp(const std::string &p)
{
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// ------------------------------------------------------------------ text ---
static void test_utf8_roundtrip()
{
    const char *samples[] = { "", "plain ascii", "日本語ＭＳ ゴシック", "Ａ・ｂ～♪", "emoji 😀 and 𠮷", "\xEF\xBB\xBF" "bom" };
    for (const char *s : samples) {
        CHECK(WideToUtf8(Utf8ToWide(s)) == s);
        CHECK(Utf16ToUtf8(Utf8ToUtf16(s)) == s);
    }
}

static void test_utf8_code_unit_counts()
{
    // U+1F600 is one wchar_t on 32-bit-wchar platforms (iOS) but a surrogate pair in UTF-16 (Windows).
    CHECK(Utf8ToUtf16("😀").size() == 2);
    CHECK(Utf8ToUtf16("😀")[0] == 0xD83D && Utf8ToUtf16("😀")[1] == 0xDE00);
    CHECK(Utf8ToUtf16("日").size() == 1 && Utf8ToUtf16("日")[0] == 0x65E5);
    if (sizeof(wchar_t) == 4) CHECK(Utf8ToWide("😀").size() == 1);
}

static void test_utf8_invalid_input()
{
    CHECK(Utf8ToWide("\xFF") == std::wstring(1, (wchar_t)0xFFFD));
    CHECK(Utf8ToWide("\xC0\x80") == std::wstring(2, (wchar_t)0xFFFD));          // overlong NUL
    CHECK(Utf8ToWide("\xED\xA0\x80").size() == 3);                              // encoded surrogate -> rejected byte by byte
    CHECK(Utf8ToWide("\xE3\x81").size() == 2);                                  // truncated sequence
    CHECK(Utf8ToWide("a\xE3" "b") == std::wstring(L"a") + (wchar_t)0xFFFD + L"b");
    // lone surrogates in UTF-16 input must not produce invalid UTF-8
    CHECK(Utf16ToUtf8(std::u16string(1, (char16_t)0xD800)) == "\xEF\xBF\xBD");
    CHECK(Utf16ToUtf8(std::u16string(1, (char16_t)0xDC00)) == "\xEF\xBF\xBD");
    CHECK(Utf16ToUtf8(std::u16string({ (char16_t)0xD800, (char16_t)'x' })) == "\xEF\xBF\xBD" "x");
}

// ------------------------------------------------------------------- png ---
static void test_png_decoding_real_skin()
{
    const char *dir = "../Resources/Skins/Default/Images/";
    struct { const char *name; int w, h; } expect[] = {
        { "Background.png", 1280, 720 },        // palette PNG
        { "Lane-Ground.png", 1024, 4224 },      // RGBA
        { "Note-Tap.png", 192, 64 } };
    for (auto &e : expect) {
        auto data = Slurp(std::string(dir) + e.name);
        int w = 0, h = 0; std::vector<uint8_t> px;
        CHECK(!data.empty() && IsPng(data.data(), data.size()));
        CHECK(DecodePng(data.data(), data.size(), &w, &h, &px));
        CHECK(w == e.w && h == e.h);
        CHECK(px.size() == size_t(e.w) * e.h * 4);
    }
    auto bg = Slurp(std::string(dir) + "Background.png");
    int w, h; std::vector<uint8_t> px; DecodePng(bg.data(), bg.size(), &w, &h, &px);
    CHECK(px[3] == 255);                                                  // palette image decodes opaque
    auto tap = Slurp(std::string(dir) + "Note-Tap.png");
    DecodePng(tap.data(), tap.size(), &w, &h, &px);
    bool hasTransparent = false; for (size_t i = 3; i < px.size(); i += 4) if (px[i] == 0) { hasTransparent = true; break; }
    CHECK(hasTransparent);                                                // straight alpha preserved
    uint8_t junk[16] = { 1, 2, 3 };
    CHECK(!DecodePng(junk, sizeof(junk), &w, &h, &px));
    auto cut = std::vector<uint8_t>(tap.begin(), tap.begin() + tap.size() / 2);
    CHECK(!DecodePng(cut.data(), cut.size(), &w, &h, &px));               // truncated file fails cleanly
}

// ------------------------------------------------------------------ font ---
static void test_sif_layout_matches_shipped_fonts()
{
    CHECK(sizeof(GlyphInfo) == 18 && sizeof(FontDataHeader) == 12);
    for (const char *name : { "Latin128.sif", "Normal32.sif", "Normal64.sif" }) {
        auto b = Slurp(std::string("../Resources/Skins/Default/Fonts/") + name);
        CHECK(b.size() > 12);
        if (b.size() <= 12) continue;
        FontDataHeader hd; std::memcpy(&hd, b.data(), sizeof(hd));
        CHECK(hd.GlyphCount > 0 && hd.ImageCount > 0);
        // after the glyph table comes: uint32 PNG size, then the PNG itself
        size_t off = sizeof(FontDataHeader) + size_t(hd.GlyphCount) * sizeof(GlyphInfo);
        CHECK(off + 12 <= b.size());
        uint32_t pngSize; std::memcpy(&pngSize, &b[off], 4);
        CHECK(IsPng(&b[off + 4], b.size() - off - 4));
        CHECK(off + 4 + pngSize <= b.size());
        // every glyph's texture index is valid
        bool ok = true;
        for (uint32_t i = 0; i < hd.GlyphCount; i++) {
            GlyphInfo g; std::memcpy(&g, &b[sizeof(FontDataHeader) + i * sizeof(GlyphInfo)], sizeof(g));
            if (g.texture >= hd.ImageCount) ok = false;
        }
        CHECK(ok);
    }
}

// ----------------------------------------------------------------- touch ---
struct Recorder {
    std::vector<std::string> log;
    TouchRouter::Sink Sink()
    {
        TouchRouter::Sink s;
        s.sliderDown = [this](int f, double x) { log.push_back("down" + std::to_string(f) + ":" + std::to_string(int(x * 1000))); };
        s.sliderMove = [this](int f, double x) { log.push_back("move" + std::to_string(f) + ":" + std::to_string(int(x * 1000))); };
        s.sliderUp = [this](int f) { log.push_back("up" + std::to_string(f)); };
        s.key = [this](TouchRouter::Key k) {
            static const char *n[] = { "UP", "DOWN", "LEFT", "RIGHT", "RETURN", "ESC", "AUTO" };
            log.push_back(std::string("key:") + n[int(k)]); };
        return s;
    }
    std::string Joined() const { std::string r; for (auto &l : log) r += l + " "; return r; }
};

static void test_touch_slider_zone()
{
    Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());
    t.Down(1, 640, 650, 0.0); t.Move(1, 768, 650, 0.1); t.Up(1, 768, 650, 0.2);
    CHECK(r.Joined() == "down1:500 move1:600 up1 ");
    r.log.clear();
    t.Down(7, 100, 400, 1.0); t.Down(8, 900, 700, 1.0); t.Up(7, 100, 400, 1.2); t.Up(8, 900, 700, 1.2);
    CHECK(r.Joined() == "down7:78 down8:703 up7 up8 ");                  // multi-touch, independent fingers
    CHECK(r.log.size() == 4);                                              // and a tap in the slider zone is NOT a Return
}

static void test_touch_gestures()
{
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // tap -> Return
      t.Down(1, 600, 200, 0); t.Up(1, 602, 201, 0.1); CHECK(r.Joined() == "key:RETURN "); }
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // between tap (0.35s) and long press (0.7s): nothing
      t.Down(1, 600, 200, 0); t.Up(1, 600, 200, 0.5); CHECK(r.log.empty()); }
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // two-finger tap -> Escape, once
      t.Down(1, 500, 200, 0); t.Down(2, 700, 200, 0.02); t.Up(1, 500, 200, 0.1); t.Up(2, 700, 200, 0.12);
      CHECK(r.Joined() == "key:ESC "); }
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // swipe right = one arrow per 56px, no Return on lift
      t.Down(1, 100, 200, 0); t.Move(1, 160, 202, 0.05); t.Move(1, 280, 204, 0.1); t.Up(1, 280, 204, 0.12);
      CHECK(r.Joined() == "key:RIGHT key:RIGHT key:RIGHT "); }
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // vertical drags
      t.Down(1, 100, 300, 0); t.Move(1, 100, 200, 0.1); t.Up(1, 100, 200, 0.2);
      CHECK(r.Joined() == "key:UP "); }
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // cancel never taps
      t.Down(1, 100, 100, 0); t.Cancel(1); t.Up(1, 100, 100, 0.05); CHECK(r.log.empty()); }
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // long press -> toggle autoplay (once, on release)
      t.Down(1, 600, 200, 0); t.Move(1, 603, 201, 0.5); CHECK(r.log.empty()); t.Up(1, 603, 201, 0.9);
      CHECK(r.Joined() == "key:AUTO "); }
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // ... but not when dragging, with two fingers, or in the slider zone
      t.Down(1, 600, 200, 0); t.Move(1, 700, 200, 0.5); t.Up(1, 700, 200, 1.0); CHECK(r.Joined() == "key:RIGHT ");
      r.log.clear(); t.Down(2, 100, 100, 2); t.Down(3, 200, 100, 2.1); t.Up(2, 100, 100, 3); t.Up(3, 200, 100, 3); CHECK(r.log.empty());
      r.log.clear(); t.Down(4, 100, 600, 4); t.Up(4, 100, 600, 5); CHECK(r.Joined() == "down4:78 up4 "); }
    { Recorder r; TouchRouter t(TouchRouterConfig(), r.Sink());           // gesture finger + slider finger don't interfere
      t.Down(1, 100, 100, 0); t.Down(2, 300, 650, 0.01); t.Up(2, 300, 650, 0.05); t.Up(1, 100, 100, 0.1);
      CHECK(r.Joined() == "down2:234 up2 key:RETURN "); }
}

static void test_key_map()
{
    CHECK(HidUsageToDxKey(0x04) == KEY_INPUT_A && HidUsageToDxKey(0x1D) == KEY_INPUT_Z);
    CHECK(HidUsageToDxKey(0x28) == KEY_INPUT_RETURN && HidUsageToDxKey(0x29) == KEY_INPUT_ESCAPE);
    CHECK(HidUsageToDxKey(0x52) == KEY_INPUT_UP && HidUsageToDxKey(0x51) == KEY_INPUT_DOWN);
    CHECK(HidUsageToDxKey(0x50) == KEY_INPUT_LEFT && HidUsageToDxKey(0x4F) == KEY_INPUT_RIGHT);
    CHECK(HidUsageToDxKey(0x36) == KEY_INPUT_COMMA);                       // slider 15's key
    CHECK(HidUsageToDxKey(0x3A) == KEY_INPUT_F1 && HidUsageToDxKey(0x3B) == KEY_INPUT_F2 && HidUsageToDxKey(0x45) == KEY_INPUT_F12);
    CHECK(HidUsageToDxKey(0x1E) == KEY_INPUT_1 && HidUsageToDxKey(0x27) == KEY_INPUT_0);
    CHECK(HidUsageToDxKey(0x7F) == -1);
    // the default slider keyboard layout from Controller.cpp is reachable from a hardware keyboard
    const int sliderKeys[16] = { KEY_INPUT_A, KEY_INPUT_Z, KEY_INPUT_S, KEY_INPUT_X, KEY_INPUT_D, KEY_INPUT_C, KEY_INPUT_F, KEY_INPUT_V,
                                 KEY_INPUT_G, KEY_INPUT_B, KEY_INPUT_H, KEY_INPUT_N, KEY_INPUT_J, KEY_INPUT_M, KEY_INPUT_K, KEY_INPUT_COMMA };
    const int hid[16] = { 0x04, 0x1D, 0x16, 0x1B, 0x07, 0x06, 0x09, 0x19, 0x0A, 0x05, 0x0B, 0x11, 0x0D, 0x10, 0x0E, 0x36 };
    for (int i = 0; i < 16; i++) CHECK(HidUsageToDxKey(hid[i]) == sliderKeys[i]);
}

// ------------------------------------------------------------ controller ---
static void test_controller_touch_and_keyboard()
{
    DxSoftRenderer r(1280, 720);
    DxInitialize(&r, 1280, 720, DecodeRaw);
    {
        ControlState cs; cs.Initialize(); cs.Update();
        CHECK(ControlState::SliderIndexForX(0.0) == 0);
        CHECK(ControlState::SliderIndexForX(0.499) == 7 && ControlState::SliderIndexForX(0.5) == 8);
        CHECK(ControlState::SliderIndexForX(1.0) == 15);                   // edge: original code produced index 16 (out of bounds)
        CHECK(ControlState::SliderIndexForX(7.0) == 15 && ControlState::SliderIndexForX(-3.0) == 0);
        CHECK(ControlState::SliderIndexForX(std::nan("")) == 0);

        cs.TouchBegan(11, 0.30);                                           // slider 4
        cs.Update();
        CHECK(cs.GetCurrentState(ControllerSource::IntegratedSliders, 4) && cs.GetTriggerState(ControllerSource::IntegratedSliders, 4));
        cs.Update();
        CHECK(cs.GetCurrentState(ControllerSource::IntegratedSliders, 4) && !cs.GetTriggerState(ControllerSource::IntegratedSliders, 4));
        cs.TouchMoved(11, 0.34);                                           // slider 5: new trigger, slider 4 released
        cs.Update();
        CHECK(cs.GetTriggerState(ControllerSource::IntegratedSliders, 5) && !cs.GetCurrentState(ControllerSource::IntegratedSliders, 4));
        cs.TouchMoved(99, 1.0);                                            // move for an unknown finger behaves like a down
        cs.Update();
        CHECK(cs.GetCurrentState(ControllerSource::IntegratedSliders, 15));
        cs.TouchEnded(11); cs.TouchEnded(99);
        cs.Update();
        for (int i = 0; i < 16; i++) CHECK(!cs.GetCurrentState(ControllerSource::IntegratedSliders, i));

        DxSetKeyHeld(KEY_INPUT_Z, true);                                   // keyboard -> slider 1
        cs.Update();
        CHECK(cs.GetTriggerState(ControllerSource::IntegratedSliders, 1) && cs.GetCurrentState(ControllerSource::RawKeyboard, KEY_INPUT_Z));
        DxSetKeyHeld(KEY_INPUT_Z, false);
        cs.SetSliderKey(0, KEY_INPUT_SPACE);
        DxPulseKey(KEY_INPUT_SPACE);
        cs.Update();
        CHECK(cs.GetTriggerState(ControllerSource::IntegratedSliders, 0));
        DxPulseKey(KEY_INPUT_RETURN);                                      // what a tap gesture produces
        cs.Update();
        CHECK(cs.GetTriggerState(ControllerSource::RawKeyboard, KEY_INPUT_RETURN));
        cs.Update();
        CHECK(!cs.GetCurrentState(ControllerSource::RawKeyboard, KEY_INPUT_RETURN));
        CHECK(!cs.GetCurrentState(ControllerSource::IntegratedSliders, 99) && !cs.GetTriggerState(ControllerSource::RawKeyboard, -1));
    }
    DxShutdown();
}

int main()
{
    RUN(test_utf8_roundtrip);
    RUN(test_utf8_code_unit_counts);
    RUN(test_utf8_invalid_input);
    RUN(test_png_decoding_real_skin);
    RUN(test_sif_layout_matches_shipped_fonts);
    RUN(test_touch_slider_zone);
    RUN(test_touch_gestures);
    RUN(test_key_map);
    RUN(test_controller_touch_and_keyboard);
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
