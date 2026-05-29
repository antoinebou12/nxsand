// Per-material tunable knobs; uploaded to the sim shader as a UBO so the
// Settings menu can tweak rules live.
//
// Field names match `PARAMS_BY_MATERIAL` keys so the JSON in
// sdmc:/switch/nxsand/physics.json deserialises cleanly into this struct.
#pragma once
#include <cstdint>

namespace nx {

struct alignas(16) PhysicsParams {
    // Fire
    float fire_speed         = 1.0f;
    float fire_smokeRate     = 0.070f;
    float fire_ignitePlant   = 0.08f;
    float fire_igniteOil     = 0.045f;
    float fire_spreadRate    = 0.040f;

    // Smoke
    float smoke_fadeRate     = 0.010f;
    float smoke_driftRate    = 0.12f;

    // Water (high spread/level = very fast, pool-like flow)
    float water_flowRate     = 1.0f;
    float water_levelRate    = 0.18f;

    // Acid
    float acid_flowRate      = 0.28f;
    float acid_wallCorrode   = 0.06f;
    float acid_stoneCorrode  = 0.045f;

    // Plant
    float plant_growthRate   = 0.12f;
    float plant_wallSupport  = 1.0f;

    // Lava: fall/spread/ignite gas; lava+water quench is hard-coded in sim.frag
    // (water -> stone, lava -> smoke), not driven by these floats.
    float lava_flowRate      = 0.16f;
    float lava_spreadRate    = 0.09f;
    float lava_igniteGas     = 0.08f;

    // Oil
    float oil_igniteRate     = 0.07f;
    float oil_floatRate      = 0.16f;

    // Ice
    float ice_meltRate       = 0.015f;
    float ice_freezeRate     = 0.030f;
};

} // namespace nx
