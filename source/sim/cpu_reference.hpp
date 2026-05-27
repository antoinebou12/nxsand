#pragma once
#include <cstdint>
#include <vector>
#include "materials.hpp"

namespace nx {

void cpu_clear(std::vector<uint8_t>& g, int w, int h, Material m);

// One Margolus phase (px,py) for sand-only golden checks.
void cpu_margolus_sand_phase(std::vector<uint8_t>& g, int w, int h, int phaseX, int phaseY,
                              uint32_t frame);

} // namespace nx
