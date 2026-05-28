#include "test_harness.hpp"
#include "sim/cpu_reference.hpp"
#include "sim/materials.hpp"
#include <vector>

void run_sim_cpu_tests(TestContext& ctx) {
    {
        const int w = 8, h = 8;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_SAND));
        nx::cpu_clear(g, w, h, nx::MAT_EMPTY);
        for (uint8_t v : g) {
            CHECK(ctx, v == static_cast<uint8_t>(nx::MAT_EMPTY));
        }
    }

    {
        const int w = 16, h = 16;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int cx = 8, cy = 8;
        g[static_cast<size_t>(cy * w + cx)] = static_cast<uint8_t>(nx::MAT_SAND);
        nx::cpu_margolus_sand_phase(g, w, h, 0, 0, 0);
        CHECK(ctx, g[static_cast<size_t>(cy * w + cx)] != static_cast<uint8_t>(nx::MAT_SAND));
    }

    {
        const int w = 16, h = 16;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int cx = 9, cy = 8;
        g[static_cast<size_t>(cy * w + cx)] = static_cast<uint8_t>(nx::MAT_SAND);
        nx::cpu_margolus_sand_phase(g, w, h, 0, 0, 0);
        CHECK(ctx, g[static_cast<size_t>(cy * w + cx)] == static_cast<uint8_t>(nx::MAT_SAND));
    }

    {
        const int w = 8, h = 8;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int x = 4, y = 4;
        g[static_cast<size_t>(y * w + x)] = static_cast<uint8_t>(nx::MAT_SAND);
        g[static_cast<size_t>((y - 1) * w + x)] = static_cast<uint8_t>(nx::MAT_WATER);
        nx::cpu_margolus_sand_phase(g, w, h, 0, 0, 0);
        CHECK(ctx, g[static_cast<size_t>(y * w + x)] != static_cast<uint8_t>(nx::MAT_SAND));
    }

    {
        const int w = 8, h = 8;
        std::vector<uint8_t> g(static_cast<size_t>(w * h), static_cast<uint8_t>(nx::MAT_EMPTY));
        const int x = 4, y = 4;
        g[static_cast<size_t>(y * w + x)] = static_cast<uint8_t>(nx::MAT_SAND);
        g[static_cast<size_t>((y - 1) * w + x)] = static_cast<uint8_t>(nx::MAT_WALL);
        nx::cpu_margolus_sand_phase(g, w, h, 0, 0, 0);
        CHECK(ctx, g[static_cast<size_t>(y * w + x)] == static_cast<uint8_t>(nx::MAT_SAND));
    }

    {
        CHECK(ctx, nx::cpu_react_ice(true, false, false, false, 0.015f, 0.f, 1.f) ==
                         nx::MAT_WATER);
        CHECK(ctx, nx::cpu_react_ice(true, false, false, false, 0.015f, 1.f, 1.f) == nx::MAT_ICE);
        CHECK(ctx, nx::cpu_react_smoke(true, false, 0.05f, 0.f, 1.f) == nx::MAT_WATER);
        CHECK(ctx, nx::cpu_react_smoke(true, false, 0.05f, 1.f, 1.f) == nx::MAT_SMOKE);
        CHECK(ctx, nx::cpu_react_smoke(false, true, 0.04f, 1.f, 0.12f) == nx::MAT_EMPTY);
        CHECK(ctx, nx::cpu_react_smoke(false, false, 0.04f, 1.f, 0.12f) == nx::MAT_SMOKE);
        CHECK(ctx, nx::cpu_liquid_level_boost_applies(0.006f, true, true, 0.f));
        CHECK(ctx, !nx::cpu_liquid_level_boost_applies(0.006f, true, false, 0.f));
        CHECK(ctx, !nx::cpu_liquid_level_boost_applies(0.006f, true, true, 1.f));
    }
}
