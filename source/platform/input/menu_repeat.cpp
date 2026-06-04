#include "menu_repeat.hpp"
#include <algorithm>

namespace nx {

namespace {

constexpr float kInitialDelaySec = 0.22f;
constexpr float kStartIntervalSec = 0.10f;
constexpr float kMinIntervalSec = 0.04f;
constexpr float kAccelPerSec = 0.02f;

float repeatInterval(float holdSecAfterDelay) {
    return std::max(kMinIntervalSec, kStartIntervalSec - holdSecAfterDelay * kAccelPerSec);
}

} // namespace

void MenuRepeatState::reset() {
    up_ = {};
    down_ = {};
    left_ = {};
    right_ = {};
}

bool MenuRepeatState::tickAxis(float dtSec, bool held, Axis& axis) {
    if (!held) {
        axis = {};
        return false;
    }

    if (!axis.active) {
        axis.active = true;
        axis.holdSec = 0.f;
        axis.repeatAcc = 0.f;
        return true;
    }

    axis.holdSec += dtSec;
    if (axis.holdSec < kInitialDelaySec) return false;

    const float afterDelay = axis.holdSec - kInitialDelaySec;
    const float interval = repeatInterval(afterDelay);
    axis.repeatAcc += dtSec;
    if (axis.repeatAcc < interval) return false;

    axis.repeatAcc -= interval;
    if (axis.repeatAcc > interval) axis.repeatAcc = 0.f;
    return true;
}

MenuRepeatPulses MenuRepeatState::tick(float dtSec, bool heldUp, bool heldDown, bool heldLeft,
                                       bool heldRight) {
    MenuRepeatPulses p{};
    if (dtSec < 0.f) dtSec = 0.f;
    p.up = tickAxis(dtSec, heldUp, up_);
    p.down = tickAxis(dtSec, heldDown, down_);
    p.left = tickAxis(dtSec, heldLeft, left_);
    p.right = tickAxis(dtSec, heldRight, right_);
    return p;
}

} // namespace nx
