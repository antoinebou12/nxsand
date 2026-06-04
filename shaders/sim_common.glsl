// Shared Margolus CA rules for sim.frag and sim.comp.
// Requires: sim_ids.glsl, uGridSize, uPhase, uFrame, PhysicsBlock, uint cell(ivec2 c).

layout(std140) uniform PhysicsBlock {
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
    float plant_bloomRate;
    float lava_flowRate;
    float lava_spreadRate;
    float lava_igniteGas;
    float oil_igniteRate;
    float oil_floatRate;
    float ice_meltRate;
    float ice_freezeRate;
    float water_levelRate;
    float sand_wetSlideScale;
    float sand_lithifyRate;
    float gunpowder_wetIgniteScale;
    float gunpowder_packBoost;
    float metal_rustRate;
    float metal_sparkRate;
    float oil_coldScale;
    float wood_charRate;
    float salt_dissolveRate;
    float ember_spawnRate;
    float ember_fadeRate;
    float ember_igniteWood;
    float coal_burnRate;
    float physics_reserved0; // was tnt_detonateRate (material removed)
    float brick_slideScale;
    float brick_cohesionScale;
};

#include "sim_rules_body.glsl"
