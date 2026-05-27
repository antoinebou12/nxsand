#pragma once
#include "physics_params.hpp"
#include "sim_state.hpp"
#include <cstdint>

namespace nx {

// std140 layout for sim.frag — keep in sync with GLSL PhysicsBlock (binding 2).
// Reaction rules (lava+water->stone, etc.) are fixed in sim.frag; only rates/tunables here.
struct alignas(16) PhysicsParamsGPU {
    float fire_speed;
    float fire_smokeRate;
    float fire_ignitePlant;
    float fire_igniteOil;
    float fire_spreadRate;

    float smoke_fadeRate;
    float smoke_driftRate;
    float water_flowRate;
    float acid_flowRate;

    float acid_wallCorrode;
    float acid_stoneCorrode;
    float plant_growthRate;
    float plant_wallSupport;

    float lava_flowRate;
    float lava_spreadRate;
    float lava_igniteGas;
    float oil_igniteRate;

    float oil_floatRate;
    float ice_meltRate;
    float ice_freezeRate;
    float _pad2;
    float _pad3;
    float _pad4;
    float _pad5;
};

static_assert(sizeof(PhysicsParamsGPU) % 16 == 0, "PhysicsParamsGPU must be 16-byte aligned");

inline PhysicsParamsGPU to_gpu(const PhysicsParams& p, int /*simW*/ = SIM_W) {
    PhysicsParamsGPU g{};
    g.fire_speed = p.fire_speed;
    g.fire_smokeRate = p.fire_smokeRate;
    g.fire_ignitePlant = p.fire_ignitePlant;
    g.fire_igniteOil = p.fire_igniteOil;
    g.fire_spreadRate = p.fire_spreadRate;
    g.smoke_fadeRate = p.smoke_fadeRate;
    g.smoke_driftRate = p.smoke_driftRate;
    g.water_flowRate = p.water_flowRate;
    g.acid_flowRate = p.acid_flowRate;
    g.acid_wallCorrode = p.acid_wallCorrode;
    g.acid_stoneCorrode = p.acid_stoneCorrode;
    g.plant_growthRate = p.plant_growthRate;
    g.plant_wallSupport = p.plant_wallSupport;
    g.lava_flowRate = p.lava_flowRate;
    g.lava_spreadRate = p.lava_spreadRate;
    g.lava_igniteGas = p.lava_igniteGas;
    g.oil_igniteRate = p.oil_igniteRate;
    g.oil_floatRate = p.oil_floatRate;
    g.ice_meltRate = p.ice_meltRate;
    g.ice_freezeRate = p.ice_freezeRate;
    return g;
}

} // namespace nx
