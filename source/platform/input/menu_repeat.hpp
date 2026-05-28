#pragma once

namespace nx {

struct MenuRepeatPulses {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

struct MenuRepeatState {
    void reset();

    MenuRepeatPulses tick(float dtSec, bool heldUp, bool heldDown, bool heldLeft, bool heldRight);

private:
    struct Axis {
        bool active = false;
        float holdSec = 0.f;
        float repeatAcc = 0.f;
    };

    bool tickAxis(float dtSec, bool held, Axis& axis);

    Axis up_{};
    Axis down_{};
    Axis left_{};
    Axis right_{};
};

} // namespace nx
