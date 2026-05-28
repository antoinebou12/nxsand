#include "test_harness.hpp"
#include "sim/physics_gpu.hpp"

void run_physics_gpu_tests(TestContext& ctx) {
    nx::PhysicsParams cpu{};
    cpu.fire_speed = 0.5f;
    cpu.ice_freezeRate = 0.04f;
    cpu.water_levelRate = 0.01f;

    const nx::PhysicsParamsGPU gpu = nx::to_gpu(cpu, nx::SIM_W);
    CHECK(ctx, gpu.fire_speed == cpu.fire_speed);
    CHECK(ctx, gpu.ice_freezeRate == cpu.ice_freezeRate);
    CHECK(ctx, gpu.water_levelRate == cpu.water_levelRate);
    CHECK(ctx, gpu.plant_wallSupport == cpu.plant_wallSupport);
    CHECK(ctx, sizeof(nx::PhysicsParamsGPU) % 16 == 0);
}
