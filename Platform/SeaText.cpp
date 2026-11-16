// SeaText.cpp - UTF-8 <-> std::wstring without OS APIs.
#include "SeaPlatform.h"

namespace sea {

namespace {

constexpr uint32_t kReplacement = 0xFFFD;

// Decodes one code point starting at s[i]; advances i. Strict: overlong forms,
// surrogates and values above U+10FFFF are rejected (-> U+FFFD, consume 1 byte).
uint32_t DecodeUtf8(const std::string &s, size_t &i)
{
    const size_t n = s.size();
    const uint8_t b0 = static_cast<uint8_t>(s[i]);
    if (b0 < 0x80) { i += 1; return b0; }

    int len = 0;
    uint32_t cp = 0, minimum = 0;
    if ((b0 & 0xE0) == 0xC0)      { len = 2; cp = b0 & 0x1F; minimum = 0x80; }
    else if ((b0 & 0xF0) == 0xE0) { len = 3; cp = b0 & 0x0F; minimum = 0x800; }
    else if ((b0 & 0xF8) == 0xF0) { len = 4; cp = b0 & 0x07; minimum = 0x10000; }
    else { i += 1; return kReplacement; }

    if (i + len > n) { i += 1; return kReplacement; }
    for (int k = 1; k < len; k++) {
        const uint8_t bk = static_cast<uint8_t>(s[i + k]);
        if ((bk & 0xC0) != 0x80) { i += 1; return kReplacement; }
        cp = (cp << 6) | (bk & 0x3F);
    }
    if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { i += 1; return kReplacement; }
    i += len;
    return cp;
}

void AppendUtf8(std::string &out, uint32_t cp)
{
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = kReplacement;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

} // namespace

namespace {

template <class CharT>
std::basic_string<CharT> Utf8ToUnits(const std::string &utf8)
{
    std::basic_string<CharT> out;
    out.reserve(utf8.size());
    size_t i = 0;
    while (i < utf8.size()) {
        const uint32_t cp = DecodeUtf8(utf8, i);
        if (sizeof(CharT) == 2 && cp >= 0x10000) {
            const uint32_t v = cp - 0x10000;
            out.push_back(static_cast<CharT>(0xD800 + (v >> 10)));
            out.push_back(static_cast<CharT>(0xDC00 + (v & 0x3FF)));
        } else {
            out.push_back(static_cast<CharT>(cp));
        }
    }
    return out;
}

template <class CharT>
std::string UnitsToUtf8(const std::basic_string<CharT> &in)
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        uint32_t cp = static_cast<uint32_t>(in[i]);
        if (sizeof(CharT) == 2 && cp >= 0xD800 && cp <= 0xDBFF) {
            // high surrogate: needs a following low surrogate
            if (i + 1 < in.size()) {
                const uint32_t lo = static_cast<uint32_t>(in[i + 1]);
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i++;
                } else cp = kReplacement;
            } else cp = kReplacement;
        }
        AppendUtf8(out, cp);          // lone low surrogates / out-of-range values become U+FFFD here
    }
    return out;
}

} // namespace

std::wstring Utf8ToWide(const std::string &utf8) { return Utf8ToUnits<wchar_t>(utf8); }
std::string WideToUtf8(const std::wstring &wide) { return UnitsToUtf8<wchar_t>(wide); }
std::u16string Utf8ToUtf16(const std::string &utf8) { return Utf8ToUnits<char16_t>(utf8); }
std::string Utf16ToUtf8(const std::u16string &utf16) { return UnitsToUtf8<char16_t>(utf16); }

} // namespace sea
