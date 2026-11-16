#pragma once

typedef struct
{
    uint16_t texture;
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
    int16_t bearX;
    int16_t bearY;
    uint16_t wholeAdvance;
    uint16_t letter;    // UTF-16 code unit.  Was wchar_t, whose size differs between platforms and
                        // is part of the on-disk .sif format (18 bytes per glyph record).
} GlyphInfo;
static_assert(sizeof(GlyphInfo) == 18, ".sif glyph records are 18 bytes");

typedef struct
{
    //イメージ名は64byteずつ
    uint16_t ImageCount;
    uint32_t GlyphCount;
    int Size;
} FontDataHeader;
static_assert(sizeof(FontDataHeader) == 12, ".sif header is 12 bytes");

class RectPacker final
{
private:
    int width;
    int height;
    int row;
    int cursorX = 0;
    int cursorY = 0;

public:
    typedef struct
    {
        int x;
        int y;
        int width;
        int height;
    } Rect;

    void Init(int w, int h, int rowh);

    Rect Insert(int w, int h);
};
