#include "SeaKeyMap.h"
#include "DxCompat/DxLib.h"

namespace sea {

int HidUsageToDxKey(int u)
{
    static const int letters[26] = {   // HID 0x04..0x1D = A..Z
        KEY_INPUT_A, KEY_INPUT_B, KEY_INPUT_C, KEY_INPUT_D, KEY_INPUT_E, KEY_INPUT_F, KEY_INPUT_G, KEY_INPUT_H,
        KEY_INPUT_I, KEY_INPUT_J, KEY_INPUT_K, KEY_INPUT_L, KEY_INPUT_M, KEY_INPUT_N, KEY_INPUT_O, KEY_INPUT_P,
        KEY_INPUT_Q, KEY_INPUT_R, KEY_INPUT_S, KEY_INPUT_T, KEY_INPUT_U, KEY_INPUT_V, KEY_INPUT_W, KEY_INPUT_X,
        KEY_INPUT_Y, KEY_INPUT_Z };
    static const int digits[10] = {    // HID 0x1E..0x27 = 1..9,0
        KEY_INPUT_1, KEY_INPUT_2, KEY_INPUT_3, KEY_INPUT_4, KEY_INPUT_5, KEY_INPUT_6, KEY_INPUT_7, KEY_INPUT_8,
        KEY_INPUT_9, KEY_INPUT_0 };
    static const int fkeys[12] = {     // HID 0x3A..0x45 = F1..F12
        KEY_INPUT_F1, KEY_INPUT_F2, KEY_INPUT_F3, KEY_INPUT_F4, KEY_INPUT_F5, KEY_INPUT_F6, KEY_INPUT_F7,
        KEY_INPUT_F8, KEY_INPUT_F9, KEY_INPUT_F10, KEY_INPUT_F11, KEY_INPUT_F12 };

    if (u >= 0x04 && u <= 0x1D) return letters[u - 0x04];
    if (u >= 0x1E && u <= 0x27) return digits[u - 0x1E];
    if (u >= 0x3A && u <= 0x45) return fkeys[u - 0x3A];
    switch (u) {
        case 0x28: return KEY_INPUT_RETURN;
        case 0x29: return KEY_INPUT_ESCAPE;
        case 0x2A: return KEY_INPUT_BACK;
        case 0x2B: return KEY_INPUT_TAB;
        case 0x2C: return KEY_INPUT_SPACE;
        case 0x2D: return KEY_INPUT_MINUS;
        case 0x2F: return KEY_INPUT_LBRACKET;
        case 0x30: return KEY_INPUT_RBRACKET;
        case 0x31: return KEY_INPUT_BACKSLASH;
        case 0x33: return KEY_INPUT_SEMICOLON;
        case 0x36: return KEY_INPUT_COMMA;
        case 0x37: return KEY_INPUT_PERIOD;
        case 0x38: return KEY_INPUT_SLASH;
        case 0x4A: return KEY_INPUT_HOME;
        case 0x4B: return KEY_INPUT_PGUP;
        case 0x4C: return KEY_INPUT_DELETE;
        case 0x4D: return KEY_INPUT_END;
        case 0x4E: return KEY_INPUT_PGDN;
        case 0x4F: return KEY_INPUT_RIGHT;
        case 0x50: return KEY_INPUT_LEFT;
        case 0x51: return KEY_INPUT_DOWN;
        case 0x52: return KEY_INPUT_UP;
        case 0xE0: return KEY_INPUT_LCONTROL;
        case 0xE1: return KEY_INPUT_LSHIFT;
        case 0xE2: return KEY_INPUT_LALT;
        case 0xE4: return KEY_INPUT_RCONTROL;
        case 0xE5: return KEY_INPUT_RSHIFT;
        case 0xE6: return KEY_INPUT_RALT;
    }
    return -1;
}

} // namespace sea
