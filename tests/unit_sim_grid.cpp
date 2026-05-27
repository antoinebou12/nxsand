#include "test_harness.hpp"
#include "sim/sim_grid_policy.hpp"

void run_sim_grid_tests(TestContext& ctx) {
#if defined(__SWITCH__)
    const auto handheld = nx::pickSimGridSize(1280, 720);
    CHECK(ctx, handheld.first == 640);
    CHECK(ctx, handheld.second == 360);

    const auto docked = nx::pickSimGridSize(1920, 1080);
    CHECK(ctx, docked.first == 960);
    CHECK(ctx, docked.second == 540);
#else
    const auto desktop = nx::pickSimGridSize(1920, 1080);
    CHECK(ctx, desktop.first == 1920);
    CHECK(ctx, desktop.second == 1080);
#endif
}
