#include "test_harness.hpp"
#include "sim/physics_gpu.hpp"

void run_physics_gpu_tests(TestContext& ctx) {
    nx::PhysicsParams cpu{};
    cpu.fire_speed = 0.5f;
    cpu.ice_freezeRate = 0.04f;
    cpu.water_levelRate = 0.01f;
    cpu.sand_lithifyRate = 0.008f;
    cpu.gunpowder_wetIgniteScale = 0.15f;
    cpu.metal_rustRate = 0.005f;
    cpu.oil_coldScale = 0.5f;
    cpu.salt_dissolveRate = 0.02f;
    cpu.fire_ignitePlant = 0.14f;
    cpu.fire_smokeRate = 0.05f;
    cpu.fire_igniteOil = 0.03f;
    cpu.ember_spawnRate = 0.002f;
    cpu.ember_fadeRate = 0.25f;
    cpu.ember_igniteWood = 0.004f;

    const nx::PhysicsParamsGPU gpu = nx::to_gpu(cpu, nx::SIM_W);
    CHECK(ctx, gpu.fire_speed == cpu.fire_speed);
    CHECK(ctx, gpu.ice_freezeRate == cpu.ice_freezeRate);
    CHECK(ctx, gpu.water_levelRate == cpu.water_levelRate);
    CHECK(ctx, gpu.sand_lithifyRate == cpu.sand_lithifyRate);
    CHECK(ctx, gpu.gunpowder_wetIgniteScale == cpu.gunpowder_wetIgniteScale);
    CHECK(ctx, gpu.metal_rustRate == cpu.metal_rustRate);
    CHECK(ctx, gpu.oil_coldScale == cpu.oil_coldScale);
    CHECK(ctx, gpu.salt_dissolveRate == cpu.salt_dissolveRate);
    CHECK(ctx, gpu.fire_ignitePlant == cpu.fire_ignitePlant);
    CHECK(ctx, gpu.fire_smokeRate == cpu.fire_smokeRate);
    CHECK(ctx, gpu.fire_igniteOil == cpu.fire_igniteOil);
    CHECK(ctx, gpu.ember_spawnRate == cpu.ember_spawnRate);
    CHECK(ctx, gpu.ember_fadeRate == cpu.ember_fadeRate);
    CHECK(ctx, gpu.ember_igniteWood == cpu.ember_igniteWood);
    CHECK(ctx, gpu.wood_charRate == cpu.wood_charRate);
    CHECK(ctx, gpu.plant_wallSupport == cpu.plant_wallSupport);
    CHECK(ctx, sizeof(nx::PhysicsParamsGPU) % 16 == 0);
}
