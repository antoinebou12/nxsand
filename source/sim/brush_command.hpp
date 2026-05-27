#pragma once
#include <cstdint>

namespace nx {

enum class BrushMode : int32_t {
    Paint = 0,
    Erase = 1,
};

// Legacy brush command shape retained for CPU stroke tests; runtime paint uses paint.frag.
struct BrushCommand {
    int32_t x = 0;
    int32_t y = 0;
    int32_t radius = 1;
    int32_t material = 0;
    int32_t mode = 0;
    float strength = 1.f;
};

static_assert(sizeof(BrushCommand) == 24, "BrushCommand must match GLSL std430");

} // namespace nx
