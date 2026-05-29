#pragma once
#include <cstdint>
#include <vector>
#include "materials.hpp"

namespace nx {

void cpu_clear(std::vector<uint8_t>& g, int w, int h, Material m);

// One Margolus phase (px,py) for sand-only golden checks.
void cpu_margolus_sand_phase(std::vector<uint8_t>& g, int w, int h, int phaseX, int phaseY,
                              uint32_t frame);

// Thresholds mirrored from shaders/sim_common.glsl (TPT-informed adjacency).
constexpr float kIceThawNearWaterRate   = 0.002f;
constexpr float kSmokeCondenseOnIceRate = 0.42f;

inline bool cpu_roll_lt(float roll01, float threshold) { return roll01 < threshold; }

// Deterministic react() slice for unit tests (roll01 in [0,1)).
inline Material cpu_react_ice(bool nearWater, bool nearFire, bool nearLava, bool nearAcid,
                              float iceMeltRate, float rollThaw, float rollMelt) {
    if (nearWater && cpu_roll_lt(rollThaw, kIceThawNearWaterRate)) return MAT_WATER;
    if (nearFire && cpu_roll_lt(rollMelt, iceMeltRate)) return MAT_WATER;
    if (nearLava && cpu_roll_lt(rollMelt, iceMeltRate > 0.08f ? iceMeltRate : 0.08f)) return MAT_WATER;
    if (nearAcid && cpu_roll_lt(rollMelt, 0.20f)) return MAT_WATER;
    return MAT_ICE;
}

inline Material cpu_react_smoke(bool nearIce, bool nearWall, bool nearStoneSand,
                                float smokeFadeRate, float rollCondense, float rollFade) {
    if (nearIce && cpu_roll_lt(rollCondense, kSmokeCondenseOnIceRate)) return MAT_WATER;
    float fade = smokeFadeRate;
    if (nearWall)
        fade = smokeFadeRate * 0.35f;
    else if (nearStoneSand)
        fade = smokeFadeRate + 0.05f > 1.f ? 1.f : smokeFadeRate + 0.05f;
    if (cpu_roll_lt(rollFade, fade)) return MAT_EMPTY;
    return MAT_SMOKE;
}

// Wide horizontal liquid spread gate (boostedFlow pre-check).
inline bool cpu_liquid_level_boost_applies(float levelRate, bool emptyOneAhead,
                                           bool emptyTwoAhead, float roll01) {
    if (levelRate <= 0.f || !emptyOneAhead || !emptyTwoAhead) return false;
    const float chance = levelRate * 3.f > 1.f ? 1.f : levelRate * 3.f;
    return cpu_roll_lt(roll01, chance);
}

// Pocket fill under a ledge (boostedFlow water-only path).
inline bool cpu_liquid_pocket_boost_applies(float levelRate, bool emptyOneAhead,
                                            bool solidBelowAhead, float roll01) {
    if (levelRate <= 0.f || !emptyOneAhead || !solidBelowAhead) return false;
    const float chance = levelRate * 6.f > 1.f ? 1.f : levelRate * 6.f;
    return cpu_roll_lt(roll01, chance);
}

} // namespace nx
