#include "test_harness.hpp"
#include "game/game_settings.hpp"
#include "sim/sim_grid_policy.hpp"

void run_sim_grid_tests(TestContext& ctx) {
    const auto balanced = nx::pickSimGridSize(1000, 562);
    CHECK(ctx, balanced.first == 640);
    CHECK(ctx, balanced.second == 360);

    const auto docked = nx::pickSimGridSize(1920, 1080);
    CHECK(ctx, docked.first == 960);
    CHECK(ctx, docked.second == 540);

    nx::PerformanceSettings perf{};
    nx::applyPerfPreset(perf, nx::PerfPreset::BatterySaver, true);
    CHECK(ctx, perf.substeps == 1);
    CHECK(ctx, nx::effectiveSubsteps(perf, true) == 1);

    nx::applyPerfPreset(perf, nx::PerfPreset::Balanced, false);
    CHECK(ctx, perf.substeps == 2);
    CHECK(ctx, nx::effectiveSubsteps(perf, false) == 2);

    nx::applyPerfPreset(perf, nx::PerfPreset::Quality, false);
    CHECK(ctx, nx::effectiveSubsteps(perf, false) == 1);
}
