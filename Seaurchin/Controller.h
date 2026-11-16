#pragma once

enum ControllerSource {
    RawKeyboard,
    RawTouch,
    IntegratedSliders,
};

struct ControllerFingerState {
    int Id;
    int SliderPosition;
};

class ControlState final {
#if defined(_WIN32)
    friend int WacomFingerCallback(WacomMTFingerCollection *fingerPacket, void *userData);
#endif

private:
    char KeyboardCurrent[256];
    char KeyboardLast[256];
    char KeyboardTrigger[256];
    char IntegratedCurrent[16];
    char IntegratedLast[16];
    char IntegratedTrigger[16];
    char SliderKeyboardNumbers[16];

    std::unordered_map<int, std::shared_ptr<ControllerFingerState>> CurrentFingers;
    std::mutex FingerMutex;

#if defined(_WIN32)
    bool IsWacomDeviceAvailable = false;
    int *WacomDeviceIds = nullptr;
    WacomMTCapability *WacomDeviceCapabilities = nullptr;
    void InitializeWacomTouchDevice();
    void UpdateWacomTouchDeviceFinger(WacomMTFingerCollection *fingers);
#endif

public:
    static const int SliderCount = 16;

    void Initialize();
    void Terminate();
    void Update();

    bool GetTriggerState(ControllerSource source, int number);
    bool GetCurrentState(ControllerSource source, int number);
    void SetSliderKey(int sliderNumber, int keyboardNumber);

    // Platform independent touch input (used by the iOS host; may be called from any thread).
    // x is the horizontal position across the whole slider area: 0.0 = left edge, 1.0 = right edge.
    void TouchBegan(int fingerId, double x);
    void TouchMoved(int fingerId, double x);
    void TouchEnded(int fingerId);

    // Maps x in [0,1] to a slider index in [0,15]; out-of-range input is clamped.
    static int SliderIndexForX(double x);
};
