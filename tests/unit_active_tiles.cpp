#include "test_harness.hpp"
#include "../source/gpu/active_tiles.hpp"
#include "../source/sim/materials.hpp"

void run_active_tiles_tests(TestContext& ctx) {
    nx::ActiveTileMap map;
    map.reset(640, 360);

    CHECK(ctx, map.activeCount() == 0);

    for (int i = 0; i < 200; ++i) {
        map.tickOptimizer(nx::ActiveTileMode::Aggressive);
    }
    CHECK(ctx, map.activeCount() == 0);

    map.markDisk(320, 180, 8);
    int bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
    CHECK(ctx, map.activeBounds(bx0, by0, bx1, by1, 1));
    map.rememberBounds(bx0, by0, bx1, by1);
    const int afterPaint = map.activeCount();
    CHECK(ctx, afterPaint > 0);
    CHECK(ctx, afterPaint < map.tileW() * map.tileH());
    std::vector<nx::ActiveTileRun> runs;
    CHECK(ctx, map.activeRuns(runs, 1));
    int runArea = 0;
    for (const auto& run : runs) {
        runArea += (run.x1 - run.x0 + 1) * (run.y1 - run.y0 + 1);
    }
    CHECK(ctx, !runs.empty());
    CHECK(ctx, runArea < map.gridW() * map.gridH());

    map.reset(160, 90);
    map.markDisk(80, 45, 4);
    CHECK(ctx, map.activeBounds(bx0, by0, bx1, by1, 1));
    map.rememberBounds(bx0, by0, bx1, by1);
    for (int i = 0; i < 200; ++i) {
        map.tickOptimizer(nx::ActiveTileMode::Aggressive);
    }
    CHECK(ctx, map.activeCount() == 0);

    map.reset(320, 192);
    map.markDisk(48, 48, 6);
    map.markDisk(272, 48, 6);
    CHECK(ctx, map.activeRuns(runs, 1));
    CHECK(ctx, runs.size() >= 2);

    map.reset(160, 90);
    for (int i = 0; i < 400; ++i) {
        map.tickOptimizer(nx::ActiveTileMode::Conservative);
    }
    CHECK(ctx, map.activeCount() == 0);

    map.reset(128, 128);
    std::vector<uint8_t> grid(static_cast<size_t>(128 * 128), nx::MAT_EMPTY);
    for (int y = 96; y < 128; ++y) {
        for (int x = 32; x < 96; ++x) {
            grid[static_cast<size_t>(y * 128 + x)] = nx::MAT_SAND;
        }
    }
    map.wakeFromGridTopDown(grid.data(), 128, 128);
    const int occupiedWake = map.activeCount();
    CHECK(ctx, occupiedWake > 0);
    CHECK(ctx, occupiedWake < map.tileW() * map.tileH());

    map.reset(640, 360);
    map.rememberBounds(100, 200, 300, 280);
    map.sleepAll();
    map.rewakeRememberedBounds(2);
    CHECK(ctx, map.activeCount() > 0);
    CHECK(ctx, map.activeCount() < map.tileW() * map.tileH());
}
