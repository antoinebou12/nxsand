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
    float fire_smokeRate     = 0.080f;
    float fire_ignitePlant   = 0.10f;
    float fire_igniteOil     = 0.025f;
    float fire_spreadRate    = 0.040f;

    // Smoke
    float smoke_fadeRate     = 0.035f;
    float smoke_driftRate    = 0.20f;

    // Water
    float water_flowRate     = 0.38f;

    // Acid
    float acid_flowRate      = 0.28f;
    float acid_wallCorrode   = 0.06f;
    float acid_stoneCorrode  = 0.045f;

    // Plant
    float plant_growthRate   = 0.02f;
    float plant_wallSupport  = 1.0f;

    // Lava: fall/spread/ignite gas; lava+water quench is hard-coded in sim.frag
    // (water -> stone, lava -> smoke), not driven by these floats.
    float lava_flowRate      = 0.22f;
    float lava_spreadRate    = 0.10f;
    float lava_igniteGas     = 0.08f;

    // Oil
    float oil_igniteRate     = 0.05f;
    float oil_floatRate      = 0.19f;

    // Ice
    float ice_meltRate       = 0.015f;
    float ice_freezeRate     = 0.030f;
};

} // namespace nx
