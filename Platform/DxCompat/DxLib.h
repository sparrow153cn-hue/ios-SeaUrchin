// DxLib.h  --  a re-implementation of the subset of the DX Library API that
// Seaurchin uses, for platforms where the real (Windows/DirectX) library does
// not exist.  Signatures, argument types and default arguments follow the real
// library so the game code compiles unchanged.
//
// Only what Seaurchin calls is provided; see docs/PORTING.md for the list.
#pragma once

#include <cstdint>
#include <cstddef>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// ----------------------------------------------------------------- types ---
typedef struct tagVECTOR   { float x, y, z; } VECTOR;
typedef struct tagCOLOR_U8 { unsigned char b, g, r, a; } COLOR_U8;   // BGRA, as in DxLib
typedef struct tagVERTEX3D {
    VECTOR   pos;
    VECTOR   norm;
    COLOR_U8 dif;
    COLOR_U8 spc;
    float    u, v;
    float    su, sv;
} VERTEX3D;

inline VECTOR VGet(float x, float y, float z) { VECTOR r = { x, y, z }; return r; }
inline VECTOR VAdd(VECTOR a, VECTOR b) { return VGet(a.x + b.x, a.y + b.y, a.z + b.z); }
inline VECTOR VSub(VECTOR a, VECTOR b) { return VGet(a.x - b.x, a.y - b.y, a.z - b.z); }

inline COLOR_U8 GetColorU8(int r, int g, int b, int a)
{
    COLOR_U8 c;
    c.r = static_cast<unsigned char>(r); c.g = static_cast<unsigned char>(g);
    c.b = static_cast<unsigned char>(b); c.a = static_cast<unsigned char>(a);
    return c;
}
// 32-bit colour mode value: 0x00RRGGBB
inline unsigned int GetColor(int r, int g, int b)
{
    return (static_cast<unsigned int>(r & 255) << 16) | (static_cast<unsigned int>(g & 255) << 8) | static_cast<unsigned int>(b & 255);
}

// ------------------------------------------------------------- constants ---
#define DX_BLENDMODE_NOBLEND 0
#define DX_BLENDMODE_ALPHA   1
#define DX_BLENDMODE_ADD     2
#define DX_BLENDMODE_SUB     3
#define DX_BLENDMODE_MUL     4

#define DX_SCREEN_BACK       (-2)

#define DX_DRAWMODE_NEAREST  0
#define DX_DRAWMODE_BILINEAR 1

// Key codes: DirectInput scan codes, exactly as DxLib defines them.
#define KEY_INPUT_BACK          0x0E
#define KEY_INPUT_TAB           0x0F
#define KEY_INPUT_RETURN        0x1C
#define KEY_INPUT_LSHIFT        0x2A
#define KEY_INPUT_RSHIFT        0x36
#define KEY_INPUT_LCONTROL      0x1D
#define KEY_INPUT_RCONTROL      0x9D
#define KEY_INPUT_ESCAPE        0x01
#define KEY_INPUT_SPACE         0x39
#define KEY_INPUT_PGUP          0xC9
#define KEY_INPUT_PGDN          0xD1
#define KEY_INPUT_END           0xCF
#define KEY_INPUT_HOME          0xC7
#define KEY_INPUT_LEFT          0xCB
#define KEY_INPUT_UP            0xC8
#define KEY_INPUT_RIGHT         0xCD
#define KEY_INPUT_DOWN          0xD0
#define KEY_INPUT_INSERT        0xD2
#define KEY_INPUT_DELETE        0xD3
#define KEY_INPUT_MINUS         0x0C
#define KEY_INPUT_YEN           0x7D
#define KEY_INPUT_PREVTRACK     0x90
#define KEY_INPUT_PERIOD        0x34
#define KEY_INPUT_SLASH         0x35
#define KEY_INPUT_LALT          0x38
#define KEY_INPUT_RALT          0xB8
#define KEY_INPUT_SCROLL        0x46
#define KEY_INPUT_SEMICOLON     0x27
#define KEY_INPUT_COLON         0x92
#define KEY_INPUT_LBRACKET      0x1A
#define KEY_INPUT_RBRACKET      0x1B
#define KEY_INPUT_AT            0x91
#define KEY_INPUT_BACKSLASH     0x2B
#define KEY_INPUT_COMMA         0x33
#define KEY_INPUT_KANJI         0x94
#define KEY_INPUT_CONVERT       0x79
#define KEY_INPUT_NOCONVERT     0x7B
#define KEY_INPUT_KANA          0x70
#define KEY_INPUT_APPS          0xDD
#define KEY_INPUT_CAPSLOCK      0x3A
#define KEY_INPUT_SYSRQ         0xB7
#define KEY_INPUT_PAUSE         0xC5
#define KEY_INPUT_LWIN          0xDB
#define KEY_INPUT_RWIN          0xDC
#define KEY_INPUT_NUMLOCK       0x45
#define KEY_INPUT_NUMPAD0       0x52
#define KEY_INPUT_NUMPAD1       0x4F
#define KEY_INPUT_NUMPAD2       0x50
#define KEY_INPUT_NUMPAD3       0x51
#define KEY_INPUT_NUMPAD4       0x4B
#define KEY_INPUT_NUMPAD5       0x4C
#define KEY_INPUT_NUMPAD6       0x4D
#define KEY_INPUT_NUMPAD7       0x47
#define KEY_INPUT_NUMPAD8       0x48
#define KEY_INPUT_NUMPAD9       0x49
#define KEY_INPUT_MULTIPLY      0x37
#define KEY_INPUT_ADD           0x4E
#define KEY_INPUT_SUBTRACT      0x4A
#define KEY_INPUT_DECIMAL       0x53
#define KEY_INPUT_DIVIDE        0xB5
#define KEY_INPUT_NUMPADENTER   0x9C
#define KEY_INPUT_F1            0x3B
#define KEY_INPUT_F2            0x3C
#define KEY_INPUT_F3            0x3D
#define KEY_INPUT_F4            0x3E
#define KEY_INPUT_F5            0x3F
#define KEY_INPUT_F6            0x40
#define KEY_INPUT_F7            0x41
#define KEY_INPUT_F8            0x42
#define KEY_INPUT_F9            0x43
#define KEY_INPUT_F10           0x44
#define KEY_INPUT_F11           0x57
#define KEY_INPUT_F12           0x58
#define KEY_INPUT_A 0x1E
#define KEY_INPUT_B 0x30
#define KEY_INPUT_C 0x2E
#define KEY_INPUT_D 0x20
#define KEY_INPUT_E 0x12
#define KEY_INPUT_F 0x21
#define KEY_INPUT_G 0x22
#define KEY_INPUT_H 0x23
#define KEY_INPUT_I 0x17
#define KEY_INPUT_J 0x24
#define KEY_INPUT_K 0x25
#define KEY_INPUT_L 0x26
#define KEY_INPUT_M 0x32
#define KEY_INPUT_N 0x31
#define KEY_INPUT_O 0x18
#define KEY_INPUT_P 0x19
#define KEY_INPUT_Q 0x10
#define KEY_INPUT_R 0x13
#define KEY_INPUT_S 0x1F
#define KEY_INPUT_T 0x14
#define KEY_INPUT_U 0x16
#define KEY_INPUT_V 0x2F
#define KEY_INPUT_W 0x11
#define KEY_INPUT_X 0x2D
#define KEY_INPUT_Y 0x15
#define KEY_INPUT_Z 0x2C
#define KEY_INPUT_0 0x0B
#define KEY_INPUT_1 0x02
#define KEY_INPUT_2 0x03
#define KEY_INPUT_3 0x04
#define KEY_INPUT_4 0x05
#define KEY_INPUT_5 0x06
#define KEY_INPUT_6 0x07
#define KEY_INPUT_7 0x08
#define KEY_INPUT_8 0x09
#define KEY_INPUT_9 0x0A

// -------------------------------------------------------- 2D graphics API ---
// Handles: > 0 valid, -1 on failure (as in DxLib).
int LoadGraph(const char *FileName /* UTF-8 */, int NotUse3DFlag = FALSE);
int LoadDivGraph(const char *FileName /* UTF-8 */, int AllNum, int XNum, int YNum,
                 int XSize, int YSize, int *HandleBuf, int NotUse3DFlag = FALSE);
int CreateGraphFromMem(const void *RGBFileImage, int RGBFileImageSize,
                       const void *AlphaFileImage = nullptr, int AlphaFileImageSize = 0,
                       int TextureFlag = TRUE, int ReverseFlag = FALSE);
int MakeScreen(int SizeX, int SizeY, int UseAlphaChannel = FALSE);
int DeleteGraph(int GrHandle, int LogOutFlag = FALSE);
int GetGraphSize(int GrHandle, int *SizeXBuf, int *SizeYBuf);

int SetDrawScreen(int DrawScreen);
int ClearDrawScreen(void *ClearRect = nullptr);
int ScreenFlip(void);

int SetDrawBlendMode(int BlendMode, int Pal);
int SetDrawBright(int RedBright, int GreenBright, int BlueBright);
int SetDrawMode(int DrawMode);

int DrawGraph(int x, int y, int GrHandle, int TransFlag);
int DrawGraphF(float xf, float yf, int GrHandle, int TransFlag);
int DrawRectGraph(int DestX, int DestY, int SrcX, int SrcY, int Width, int Height,
                  int GraphHandle, int TransFlag, int TurnFlag = FALSE);
int DrawRotaGraph3F(float xf, float yf, float cxf, float cyf, double ExtRateX, double ExtRateY,
                    double Angle, int GrHandle, int TransFlag, int TurnFlag = FALSE);
// NOTE: cxf/cyf are relative to the top-left of the *source rectangle*.
int DrawRectRotaGraph3F(float xf, float yf, int SrcX, int SrcY, int Width, int Height,
                        float cxf, float cyf, double ExtRateX, double ExtRateY, double Angle,
                        int GraphHandle, int TransFlag, int TurnFlag = FALSE);
int DrawModiGraphF(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
                   int GrHandle, int TransFlag);
int DrawRectModiGraphF(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4,
                       int SrcX, int SrcY, int SrcWidth, int SrcHeight, int GrHandle, int TransFlag);

int DrawPixel(int x, int y, unsigned int Color);
int DrawBox(int x1, int y1, int x2, int y2, unsigned int Color, int FillFlag);
int DrawBoxAA(float x1, float y1, float x2, float y2, unsigned int Color, int FillFlag, float LineThickness = 1.0f);
int DrawLineAA(float x1, float y1, float x2, float y2, unsigned int Color, float Thickness = 1.0f);
int DrawOvalAA(float x, float y, float rx, float ry, int posnum, unsigned int Color, int FillFlag, float LineThickness = 1.0f);

// -------------------------------------------------------------- 3D API ---
int SetUseZBuffer3D(int Flag);
int SetWriteZBuffer3D(int Flag);
int SetUseLighting(int Flag);
int SetCameraPositionAndTarget_UpVecY(VECTOR Position, VECTOR Target);
VECTOR ConvWorldPosToScreenPos(VECTOR WorldPos);
int DrawTriangle3D(VECTOR Pos1, VECTOR Pos2, VECTOR Pos3, unsigned int Color, int FillFlag);
int DrawPolygonIndexed3D(const VERTEX3D *Vertex, int VertexNum, const unsigned short *Indices,
                         int PolygonNum, int GrHandle, int TransFlag);

// ---------------------------------------------------------- input / misc ---
int GetHitKeyStateAll(char *KeyStateArray /* 256 bytes */);
int CheckHitKey(int KeyCode);

// Text-input widgets: Seaurchin creates one but never reads meaningful data
// from it (the script-facing factory returns nullptr), so these are inert.
int MakeKeyInput(int MaxStrLength, int CancelValidFlag, int SingleCharOnlyFlag, int NumCharOnlyFlag,
                 int DoubleCharOnlyFlag = FALSE, int EnableIMEFlag = FALSE);
int DeleteKeyInput(int InputHandle);
int SetActiveKeyInput(int InputHandle);
int CheckKeyInput(int InputHandle);
int GetKeyInputString(char *StrBuffer, int InputHandle);
int GetKeyInputCursorPosition(int InputHandle);
int GetKeyInputSelectArea(int *SelectStart, int *SelectEnd, int InputHandle);

// Async-load flag: loads are synchronous here (the call is accepted and ignored).
int SetUseASyncLoadFlag(int Flag);

// =========================================================================
//  Platform-integration API (not part of real DxLib).  Used by the host app.
// =========================================================================
#include <vector>
#include <functional>

namespace sea {

struct DxRenderer;   // see DxRenderer.h

// Decodes an image file in memory to straight-alpha RGBA8.
typedef std::function<bool(const uint8_t *data, size_t size, int *w, int *h, std::vector<uint8_t> *rgba)> DxImageDecoder;

// Must be called before any other DxLib call.  Ownership of `renderer` stays with the caller.
void DxInitialize(DxRenderer *renderer, int virtualWidth, int virtualHeight, DxImageDecoder decoder);
void DxShutdown();

// Projection used by the 3D calls. Defaults: 60deg vertical FOV (see docs/PORTING.md, "Calibration").
void DxSetProjection(float verticalFovDegrees, float nearClip, float farClip);

// Keyboard state injection.  Held keys stay down until released; a "pulse" is
// seen as pressed by exactly one GetHitKeyStateAll() call (for tap/swipe gestures).
void DxSetKeyHeld(int dxKeyCode, bool held);
void DxPulseKey(int dxKeyCode);

} // namespace sea
