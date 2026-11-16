// SeaKeyMap.h - hardware keyboard (USB HID usage IDs, as delivered by GameController's GCKeyCode)
// to DxLib/DirectInput key codes.
#pragma once

namespace sea {
// Returns the DxLib KEY_INPUT_* code, or -1 for keys Seaurchin has no use for.
int HidUsageToDxKey(int hidUsage);
}
