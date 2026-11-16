// SeaTouchRouter.h - turns raw touches into what Seaurchin understands.
//
// Seaurchin's input model is a 16-slider touch strip (gameplay) plus keyboard
// keys (menus: arrows / Return / Escape).  On a phone/tablet there is no
// strip and no keyboard, so the screen is split in two:
//
//   * lower part (y >= sliderZoneTop):  finger x maps to slider 0..15  -> sliders
//   * upper part:                       gestures -> key presses
//         tap                -> Return
//         two-finger tap     -> Escape
//         swipe / drag       -> arrow keys (one key press per `swipeStep` of travel)
//         long press         -> ToggleAutoplay (the skin switches autoplay with the A / M keys on the
//                               song-select screen, and autoplay is the default, so touch needs its own way)
//
// Pure logic, no platform dependencies; unit-tested on Linux.
#pragma once

#include <functional>
#include <unordered_map>

namespace sea {

struct TouchRouterConfig {
    double virtualWidth = 1280;
    double virtualHeight = 720;
    double sliderZoneTop = 0.45;      // fraction of the height where the slider zone starts
    double swipeStep = 56;            // virtual pixels of travel per arrow key press
    double tapMaxDuration = 0.35;     // seconds
    double tapMaxMove = 28;           // virtual pixels
    double longPressMin = 0.7;        // seconds a single still finger must stay down for ToggleAutoplay
};

class TouchRouter {
public:
    enum class Key { Up, Down, Left, Right, Return, Escape, ToggleAutoplay };

    struct Sink {
        std::function<void(int finger, double x01)> sliderDown;
        std::function<void(int finger, double x01)> sliderMove;
        std::function<void(int finger)> sliderUp;
        std::function<void(Key key)> key;
    };

    TouchRouter(const TouchRouterConfig &cfg, Sink sink) : cfg_(cfg), sink_(std::move(sink)) {}

    // Coordinates are in virtual pixels (0..virtualWidth, 0..virtualHeight); time in seconds.
    void Down(int finger, double x, double y, double t);
    void Move(int finger, double x, double y, double t);
    void Up(int finger, double x, double y, double t);
    void Cancel(int finger);

private:
    struct Finger {
        bool slider = false;
        double startX = 0, startY = 0, startT = 0;   // for tap detection
        double anchorX = 0, anchorY = 0;             // for swipe stepping (moves after each step)
        double lastX = 0, lastY = 0;
        bool moved = false;                          // exceeded tapMaxMove at any time
    };

    void Emit(Key k) { if (sink_.key) sink_.key(k); }
    void EndGestureFinger(int finger, const Finger &f, double t);

    TouchRouterConfig cfg_;
    Sink sink_;
    std::unordered_map<int, Finger> fingers_;
    int gestureFingers_ = 0;       // currently down gesture fingers
    int groupPeak_ = 0;            // max simultaneous gesture fingers in the current group
    bool groupMoved_ = false;      // any finger of the group moved/swiped
    double groupStart_ = 0;
};

} // namespace sea
