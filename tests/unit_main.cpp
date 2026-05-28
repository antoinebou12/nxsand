// CPU-only unit tests (no GPU/SDL). Run via `make test` or `make golden`.
#include "test_harness.hpp"
#include <cstdio>

void run_materials_tests(TestContext& ctx);
void run_sim_cpu_tests(TestContext& ctx);
void run_physics_tests(TestContext& ctx);
void run_base64_tests(TestContext& ctx);
void run_physics_gpu_tests(TestContext& ctx);
void run_physics_settings_tests(TestContext& ctx);
void run_layout_tests(TestContext& ctx);
void run_sim_grid_tests(TestContext& ctx);
void run_settings_tests(TestContext& ctx);
void run_brush_stroke_tests(TestContext& ctx);
void run_active_tiles_tests(TestContext& ctx);
void run_tpt_import_tests(TestContext& ctx);
void run_perf_preset_physics_tests(TestContext& ctx);

int main() {
    TestContext ctx{};
    std::printf("unit tests:\n");
    RUN_SUITE(ctx, "materials", run_materials_tests);
    RUN_SUITE(ctx, "sim_cpu", run_sim_cpu_tests);
    RUN_SUITE(ctx, "physics", run_physics_tests);
    RUN_SUITE(ctx, "base64", run_base64_tests);
    RUN_SUITE(ctx, "physics_gpu", run_physics_gpu_tests);
    RUN_SUITE(ctx, "physics_settings", run_physics_settings_tests);
    RUN_SUITE(ctx, "layout", run_layout_tests);
    RUN_SUITE(ctx, "sim_grid", run_sim_grid_tests);
    RUN_SUITE(ctx, "settings", run_settings_tests);
    RUN_SUITE(ctx, "brush_stroke", run_brush_stroke_tests);
    RUN_SUITE(ctx, "active_tiles", run_active_tiles_tests);
    RUN_SUITE(ctx, "tpt_import", run_tpt_import_tests);
    RUN_SUITE(ctx, "perf_preset_physics", run_perf_preset_physics_tests);

    if (ctx.failed > 0) {
        std::fprintf(stderr, "unit tests: %d failure(s)\n", ctx.failed);
        return 1;
    }
    std::printf("unit tests: OK (%d suites)\n", 13);
    return 0;
}
