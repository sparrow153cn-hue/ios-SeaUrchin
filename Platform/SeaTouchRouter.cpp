#include "SeaTouchRouter.h"

#include <algorithm>
#include <cmath>

namespace sea {

void TouchRouter::Down(int id, double x, double y, double t)
{
    Cancel(id);                                          // a re-used id replaces any stale finger
    Finger f;
    f.slider = y >= cfg_.virtualHeight * cfg_.sliderZoneTop;
    f.startX = f.anchorX = f.lastX = x;
    f.startY = f.anchorY = f.lastY = y;
    f.startT = t;
    fingers_[id] = f;

    if (f.slider) {
        if (sink_.sliderDown) sink_.sliderDown(id, x / cfg_.virtualWidth);
        return;
    }
    if (gestureFingers_ == 0) { groupPeak_ = 0; groupMoved_ = false; groupStart_ = t; }
    gestureFingers_++;
    groupPeak_ = std::max(groupPeak_, gestureFingers_);
}

void TouchRouter::Move(int id, double x, double y, double t)
{
    (void)t;
    auto it = fingers_.find(id);
    if (it == fingers_.end()) return;
    Finger &f = it->second;
    f.lastX = x; f.lastY = y;

    if (f.slider) {
        if (sink_.sliderMove) sink_.sliderMove(id, x / cfg_.virtualWidth);
        return;
    }
    if (std::hypot(x - f.startX, y - f.startY) > cfg_.tapMaxMove) { f.moved = true; groupMoved_ = true; }

    // Dragging scrolls: one arrow press per swipeStep of travel along the dominant axis.
    for (;;) {
        const double dx = x - f.anchorX, dy = y - f.anchorY;
        if (std::max(std::fabs(dx), std::fabs(dy)) < cfg_.swipeStep) break;
        if (std::fabs(dx) >= std::fabs(dy)) {
            Emit(dx > 0 ? Key::Right : Key::Left);
            f.anchorX += dx > 0 ? cfg_.swipeStep : -cfg_.swipeStep;
        } else {
            Emit(dy > 0 ? Key::Down : Key::Up);
            f.anchorY += dy > 0 ? cfg_.swipeStep : -cfg_.swipeStep;
        }
        groupMoved_ = true;
    }
}

void TouchRouter::EndGestureFinger(int id, const Finger &f, double t)
{
    (void)id; (void)f;
    gestureFingers_--;
    if (gestureFingers_ > 0) return;
    // Whole group lifted: was it a tap?
    const bool quick = (t - groupStart_) <= cfg_.tapMaxDuration;
    if (!groupMoved_) {
        if (quick) Emit(groupPeak_ >= 2 ? Key::Escape : Key::Return);
        else if (groupPeak_ == 1 && (t - groupStart_) >= cfg_.longPressMin) Emit(Key::ToggleAutoplay);
    }
    groupPeak_ = 0;
    groupMoved_ = false;
    gestureFingers_ = 0;
}

void TouchRouter::Up(int id, double x, double y, double t)
{
    auto it = fingers_.find(id);
    if (it == fingers_.end()) return;
    Finger f = it->second;
    f.lastX = x; f.lastY = y;
    fingers_.erase(it);

    if (f.slider) {
        if (sink_.sliderUp) sink_.sliderUp(id);
        return;
    }
    EndGestureFinger(id, f, t);
}

void TouchRouter::Cancel(int id)
{
    auto it = fingers_.find(id);
    if (it == fingers_.end()) return;
    const bool slider = it->second.slider;
    fingers_.erase(it);
    if (slider) {
        if (sink_.sliderUp) sink_.sliderUp(id);
    } else {
        // a cancelled gesture never counts as a tap
        groupMoved_ = true;
        if (--gestureFingers_ <= 0) { gestureFingers_ = 0; groupPeak_ = 0; groupMoved_ = false; }
    }
}

} // namespace sea
