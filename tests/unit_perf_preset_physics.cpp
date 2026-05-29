#include "test_harness.hpp"
#include "game/game_settings.hpp"

void run_perf_preset_physics_tests(TestContext& ctx) {
    nx::PhysicsParams p{};
    p.water_levelRate = 0.012f;

    nx::applyPerfPresetPhysics(p, nx::PerfPreset::BatterySaver);
    CHECK(ctx, p.water_levelRate == 0.18f);

    nx::applyPerfPresetPhysics(p, nx::PerfPreset::Balanced);
    CHECK(ctx, p.water_levelRate == 0.18f);

    p.water_levelRate = 0.012f;
    nx::applyPerfPresetPhysics(p, nx::PerfPreset::Manual);
    CHECK(ctx, p.water_levelRate == 0.012f);

    nx::applyPerfPresetPhysics(p, nx::PerfPreset::Quality);
    CHECK(ctx, p.water_levelRate == 0.18f);
}
